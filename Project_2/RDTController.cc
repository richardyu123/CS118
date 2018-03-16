#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>

#include "Parameters.h"

#include "RDTController.h"


using namespace std;
using namespace std::chrono;

RDTController::RDTController(const int sock_fd)
    : front_packet(nullptr), cli_len(sizeof(cli_addr)), sock_fd(sock_fd),
      is_connected(true), offset(0), next_seq_num(0), send_base(0),
      receive_base(0) {}

RDTController::~RDTController() {
    if (front_packet != nullptr) {
        delete front_packet;
    }
}

/*
 * Receive num_bytes amount of bytes from the socket.
 * Fill str_buffer with the read bytes.
 */
void RDTController::Receive(std::string& str_buffer, size_t num_bytes) {
    str_buffer.resize(num_bytes);
    auto str_iter = str_buffer.begin();
    size_t curr = 0;

    // Receive pre-buffered packets.
    while(!received.empty() && receive_base == received.front().num) {
        auto data = received.front().pkt.GetData();
        size_t bytes = min(num_bytes - curr, static_cast<size_t>(distance(
                        data.begin() + offset, data.end())));
        for (auto iter = data.begin() + offset; iter != data.begin() +
             offset + bytes; iter++) {
            *str_iter = *iter;
            str_iter++;
        }
        curr += bytes;

        if (bytes + offset == data.size()) {
            received.pop_front();
            receive_base += data.size() + parameters::HEADER_SIZE;
            offset = 0;
        } else {
            offset = bytes;
            return;
        }

    }
    ssize_t n_bytes;
    char buffer[parameters::MAX_PACKET_LEN];

    // Receive until the output buffer is filled.
    if (!ConfigureTimeout(0, 0)) { return; }
    while (curr < num_bytes) {
        memset((void*)buffer, 0, parameters::MAX_PACKET_LEN);
        Packet pkt;

        // If there is some packet read from before -- read that, else
        // receive a new packet.
        if (front_packet) {
            pkt = *front_packet;
            delete front_packet;
            front_packet = nullptr;
        } else {
            n_bytes = recvfrom(sock_fd, buffer, parameters::MAX_PACKET_LEN,
                            0, (struct sockaddr*)&cli_addr, &cli_len);
            pkt = Packet(buffer, n_bytes);
            PrintPacketInfo(pkt, RECEIVER, false);
        }
        if (pkt.GetType() != Packet::DATA) { continue; }

        uint64_t seq_num = pkt.GetPacketNumber() + CalculateOffset(receive_base);
        uint32_t seq = receive_base % parameters::MAX_SEQ_NUM;
        if (seq + parameters::WINDOW_SIZE > parameters::MAX_SEQ_NUM) {
            if (pkt.GetPacketNumber() < seq && pkt.GetPacketNumber() <= 
                    parameters::WINDOW_SIZE) {
                seq_num = pkt.GetPacketNumber() + receive_base +
                    parameters::MAX_SEQ_NUM - seq;
            }
        }

        // If the received packet falls within the window range.
        if (seq_num >= receive_base && seq_num <= receive_base +
                parameters::WINDOW_SIZE - 1) {
            bool retrans = false;
            auto iter = received.begin();

            // Place packet into received list.
            while (iter != received.end()) {
                if (iter->num >= seq_num) { break; }
                iter++;
            }
            if (iter == received.end()) {
                received.emplace(iter, packet_seq_t(pkt, seq_num));
            } else if (iter != received.end()) {
                if (iter->num != seq_num) {
                    received.emplace(iter, packet_seq_t(pkt, seq_num));
                } else {
                    retrans = true;
                }
            }

            // Prepare and send ACK for the packet.
            Packet acket(Packet::ACK, pkt.GetPacketNumber(),
                            parameters::WINDOW_SIZE, nullptr, 0);
            PrintPacketInfo(acket, SENDER, retrans);
            sendto(sock_fd, acket.GetPacketData().data(),
                    acket.GetPacketLength(), 0, (struct sockaddr*)&cli_addr,
                    cli_len);

            // Update the window.
            while(receive_base == received.front().num) {
                auto data = received.front().pkt.GetData();
                size_t bytes = min(static_cast<size_t>(distance(
                                    data.begin(), data.end())), num_bytes -
                                    curr);
                for (auto iter = data.begin(); iter != data.begin() + bytes;
                        iter++) {
                    *str_iter = *iter;
                    str_iter++;
                }
                curr += bytes;
                if (bytes == data.size()) {
                    received.pop_front();
                    receive_base += data.size() + parameters::HEADER_SIZE;
                    offset = 0;
                } else {
                    offset = bytes;
                    return;
                }
            }
        } else {
            /*
             * Prepare and send ACK, but do not place into received list if the
             * packet does not fall within the receive window.
             */
            Packet acket(Packet::ACK, pkt.GetPacketNumber(),
                            parameters::WINDOW_SIZE, nullptr, 0);
            PrintPacketInfo(acket, SENDER, (seq_num < receive_base));
            sendto(sock_fd, acket.GetPacketData().data(),
                    acket.GetPacketLength(), 0, (struct sockaddr*)&cli_addr,
                    cli_len);
        }
    }
}

/*
 * Prepares and sends data through packets.
 * Waits for ACKs when the send window is full.
 */
void RDTController::Send(const std::string& data, uint32_t max_size) {
    auto begin = data.begin();
    auto end = data.end();
    if (max_size != 0) {
        end = begin + max_size;
    }
    if (!ConfigureTimeout(0, parameters::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    // Data structures:
    unordered_map<uint64_t, Packet> packets; // Seq_num -> packet.
    unordered_map<uint64_t, milliseconds> timestamps; // Seq_num -> timestamp.
    list<uint64_t> unacked_seqs; // List of unacked packets' seq_nums.
    unordered_map<uint64_t, bool> acks; // Seq_num -> acked.
    
    while (true) {
        /*
         * While there is still room in the window to send packets.
         * Generate a packet.
         * Send the packet.
         * Update the sequence number.
         */
        bool done_sending = false;
        char buf[parameters::MAX_PACKET_LEN - parameters::HEADER_SIZE];
        while (next_seq_num < send_base + parameters::WINDOW_SIZE) {
            size_t data_size = min<size_t>(parameters::MAX_PACKET_LEN - 
                                   parameters::HEADER_SIZE, send_base +
                                   parameters::WINDOW_SIZE - next_seq_num);
            if (data_size == 0) {
                done_sending = true;
                break;
            }

            // Fill packet with input string.
            memset((void*)buf, 0, data_size);
            size_t count = 0;
            while (begin != end && count < data_size) {
                buf[count] = *begin;
                begin++;
                count++;
            }

            // If there is no more string left, break.
            if (count == 0) {
                done_sending = true;
                break;
            }

            Packet pkt = Packet(Packet::DATA, next_seq_num %
                                parameters::MAX_SEQ_NUM,
                                parameters::WINDOW_SIZE, buf, count);
            
            // Sent packet needs to be acked.
            unacked_seqs.push_back(next_seq_num);
            packets[next_seq_num] = pkt;
            
            auto duration = system_clock::now().time_since_epoch();
            auto timestamp = duration_cast<milliseconds>(duration);
            timestamps[next_seq_num] = timestamp;
            
            SendPacket(pkt, false);
            next_seq_num += count + parameters::HEADER_SIZE;
        }
        
        if (packets.empty() && done_sending) {
            break;
        }
        
        /*
         * Receive packets -- if ACK of a sent packet, add to acks, and remove
         * from list of unacked packets. If packet is not an ACK, save for a 
         * possible read later.
         */
        char buffer[parameters::MAX_PACKET_LEN];
        memset(buffer, 0, parameters::MAX_PACKET_LEN);
        ssize_t num_bytes = recvfrom(sock_fd, buffer, parameters::MAX_PACKET_LEN,
                                  0, (struct sockaddr*)&cli_addr, &cli_len);

        if (num_bytes > 0) {
            Packet pkt(buffer, num_bytes);
            
            if (pkt.GetType() == Packet::ACK) {
                auto pkt_num = pkt.GetPacketNumber();
                // Get the ack number for the packet based on the send base.
                auto ack_num = pkt_num + CalculateOffset(send_base);
                auto curr_seq = send_base % parameters::MAX_SEQ_NUM;
                if (curr_seq + parameters::WINDOW_SIZE >
                        parameters::MAX_SEQ_NUM) {
                    if (pkt_num <= parameters::WINDOW_SIZE &&
                        pkt_num < curr_seq) {
                        ack_num = pkt_num + send_base +
                            parameters::MAX_SEQ_NUM - curr_seq;
                    }
                }

                // Remove packet from unacked list if the ack number matches.
                if (send_base <= ack_num && ack_num <= send_base +
                        parameters::WINDOW_SIZE) {
                    PrintPacketInfo(pkt, RECEIVER, false);
                    acks[ack_num] = true;
                    for (auto iter = unacked_seqs.begin(); iter !=
                            unacked_seqs.end(); iter++) {
                        if (*iter == ack_num) {
                            unacked_seqs.erase(iter);
                            break;
                        } 
                    }
                    timestamps.erase(ack_num);
                }
            } else {
                PrintPacketInfo(pkt, RECEIVER, false);
                auto ack_num = pkt.GetPacketNumber();
                if (ack_num == receive_base % parameters::MAX_SEQ_NUM) {
                    front_packet = new Packet(pkt);
                    return;
                }
            }
        }

        // Increase the send base if the packet at the base has been acked.
        while (acks.count(send_base) != 0 && acks[send_base]) {
            uint64_t tmp = send_base;
            send_base += packets[send_base].GetPacketLength();
            acks[tmp] = false;
            packets.erase(tmp);
        }

        /*
         * Resend timed out packets.
         * Reset timestamps on resent packets.
         */
        for (auto seq_n : unacked_seqs) {
            milliseconds now = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch());
            if (abs((now - timestamps[seq_n]).count()) >=
                static_cast<int>(parameters::RETRANS_TIMEOUT)) {
                timestamps[seq_n] = duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch());
                SendPacket(packets[seq_n], true);
            }
        }
    }
}

bool RDTController::connected() const { return is_connected; }

bool RDTController::ConfigureTimeout(int sec, int usec) {
    struct timeval t_val;
    t_val.tv_sec = sec;
    t_val.tv_usec = usec;

    auto rc = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &t_val,
                          sizeof(t_val));
    if (rc != 0) { PrintErrorAndDC("Setting timeout value"); }
    return (rc == 0);
}

void RDTController::PrintErrorAndDC(const string& msg) {
    fprintf(stderr, "ERROR: %s.\n", msg.c_str());
    is_connected = false;
}

void RDTController::PrintPacketInfo(const Packet& packet, rec_or_sender_t rs,
                                    bool retrans) {
    if (rs == RECEIVER) {
        retrans = false;
        cout << "Receiving";
    } else { // rs == SENDER.
        cout << "Sending";
    }
    cout << " packet" << " " << packet.GetPacketNumber() <<
        " " << packet.GetWindowSize();
    if (retrans) {
        cout << " Retransmission";
    }
    if (packet.GetType() != Packet::DATA) {
        cout << " " << packet.TypeToString();
    }
    cout << endl;
}

uint64_t RDTController::CalculateOffset(uint64_t num) {
    int rem = num % parameters::MAX_SEQ_NUM;
    if (rem != 0) { return num - rem; }
    return num;
}
