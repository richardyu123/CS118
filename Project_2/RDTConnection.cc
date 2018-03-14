include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unordered_map>

#include "Constants.h"

#include "RDTConnection.h"


using namespace std;
using namespace std::chrono;

RDTConnection::RDTConnection(const int sock_fd)
    : front_packet(nullptr), cli_len(sizeof(cli_addr)), sock_fd(sock_fd),
      is_connected(true), offset(0), next_seq_num(0), send_base(0),
      receive_base(0) {}

RDTConnection::~RDTConnection() {
    if (front_packet != nullptr) {
        delete front_packet;
    }
}

/*
 * Read num_bytes amount of bytes from the socket.
 * Fill str_buffer with the read bytes.
 */
void RDTConnection::Read(std::string& str_buffer, size_t num_bytes) {
    str_buffer.resize(num_bytes);
    auto str_iter = str_buffer.begin();
    size_t curr = 0;

    // Read pre-buffered packets.
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
            receive_base += data.size();
            offset = 0;
        } else {
            offset = bytes;
            return;
        }

    }
    ssize_t len;
    char buffer[constants::MAX_PACKET_LEN];

    // Read until the output buffer is filled.
    if (!ConfigureTimeout(0, 0)) { return; }
    while (curr < num_bytes) {
        memset((void*)buffer, 0, constants::MAX_PACKET_LEN);
        Packet pkt;

        // If there is some packet read from before -- read that, else
        // receive a new packet.
        if (front_packet) {
            pkt = *front_packet;
            delete front_packet;
            front_packet = nullptr;
        } else {
            len = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN,
                            0, (struct sockaddr*)&cli_addr, &cli_len);
            pkt = Packet(buffer, len);
            PrintPacketInfo(pkt, RECEIVER, false);
        }
        if (pkt.GetPacketType() != Packet::NONE) { continue; }

        uint64_t seq_num = pkt.GetPacketNumber() + CalculateOffset(receive_base);
        uint32_t seq = receive_base % constants::MAX_SEQ_NUM;
        if (seq + constants::WINDOW_SIZE > constants::MAX_SEQ_NUM) {
            if (pkt.GetPacketNumber() < seq && pkt.GetPacketNumber() <= 
                    constants::WINDOW_SIZE) {
                seq_num = pkt.GetPacketNumber() + receive_base +
                    constants::MAX_SEQ_NUM - seq;
            }
        }

        // If the received packet falls within the window range.
        if (seq_num >= receive_base && seq_num <= receive_base +
                constants::WINDOW_SIZE - 1) {
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
                            constants::WINDOW_SIZE, nullptr, 0);
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
                    receive_base += data.size();
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
                            constants::WINDOW_SIZE, nullptr, 0);
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
void RDTConnection::Write(const std::string& data, uint32_t max_size) {
    auto begin = data.begin();
    auto end = data.end();
    if (max_size != 0) {
        end = begin + max_size;
    }
    if (!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    // Data structures:
    unordered_map<uint64_t, Packet> packets; // Seq_num -> packet.
    unordered_map<uint64_t, milliseconds> timestamps; // Seq_num -> timestamp.
    list<uint64_t> packet_list; // List of unacked packets' seq_nums.
    unordered_map<uint64_t, bool> acks; // Seq_num -> acked.
    
    while (true) {
        /*
         * Resend timed out packets.
         * Reset timestamps on resent packets.
         */
        milliseconds cur_time;
        for (auto seq_n : packet_list) {
            cur_time = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch());
            if (abs(duration_cast<milliseconds>(
                            cur_time - timestamps[seq_n]).count()) >=
                static_cast<int>(constants::RETRANS_TIMEOUT)) {
                timestamps[seq_n] = duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch());
                SendPacket(packets[seq_n], true);
            }
        }

        /*
         * While there is still room in the window to send packets.
         * Generate a packet.
         * Send the packet.
         * Update the sequence number.
         */
        bool done_sending = false;
        while (next_seq_num < send_base + constants::WINDOW_SIZE) {
            size_t data_size = min<size_t>(constants::MAX_PACKET_LEN - 
                                   constants::HEADER_SIZE, send_base +
                                   constants::WINDOW_SIZE - next_seq_num);
            if (data_size == 0) {
                done_sending = true;
                break;
            }

            // Fill packet with input string.
            char buf[data_size];
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

            Packet pkt = Packet(Packet::NONE, next_seq_num %
                                constants::MAX_SEQ_NUM,
                                constants::WINDOW_SIZE, buf, count);
            
            // Sent packet needs to be acked.
            packet_list.push_back(next_seq_num);
            packets[next_seq_num] = pkt;
            
            auto duration = system_clock::now().time_since_epoch();
            auto timestamp = duration_cast<milliseconds>(duration);
            timestamps[next_seq_num] = timestamp;
            
            SendPacket(pkt, false);
            next_seq_num += count;
        }
        
        if (done_sending && packets.empty()) {
            break;
        }
        
        /*
         * Receive packets -- if ACK of a sent packet, add to acks, and remove
         * from list of unacked packets. If packet is not an ACK, save for a 
         * possible read later.
         */
        char buffer[constants::MAX_PACKET_LEN];
        memset(buffer, 0, constants::MAX_PACKET_LEN);
        auto num_bytes = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN,
                                  0, (struct sockaddr*)&cli_addr, &cli_len);

        if (num_bytes > 0) {
            Packet pkt(buffer, num_bytes);

            if (pkt.GetPacketType() == Packet::ACK) {
                // Get the ack number for the packet based on the send base.
                auto ack_num = pkt.GetPacketNumber() + CalculateOffset(send_base);
                auto curr_seq = send_base % constants::MAX_SEQ_NUM;
                if (curr_seq + constants::WINDOW_SIZE >
                        constants::MAX_SEQ_NUM) {
                    if (pkt.GetPacketNumber() < curr_seq &&
                            pkt.GetPacketNumber() <= constants::WINDOW_SIZE) {
                        ack_num = pkt.GetPacketNumber() + send_base +
                            constants::MAX_SEQ_NUM - curr_seq;
                    }
                }

                // Remove packet from unacked list if the ack number matches.
                if (send_base <= ack_num && ack_num <= send_base +
                        constants::WINDOW_SIZE) {
                    PrintPacketInfo(pkt, RECEIVER, false);
                    acks[ack_num] = true;
                    for (auto iter = packet_list.begin(); iter !=
                            packet_list.end(); iter++) {
                        if (*iter == ack_num) {
                            packet_list.erase(iter);
                            break;
                        } 
                    }
                    timestamps.erase(ack_num);
                }
            } else {
                PrintPacketInfo(pkt, RECEIVER, false);
                auto ack_num = pkt.GetPacketNumber();
                if (ack_num == receive_base % constants::MAX_SEQ_NUM) {
                    front_packet = new Packet(pkt);
                    return;
                }
            }
        }

        // Increase the send base if the packet at the base has been acked.
        while (acks.count(send_base) != 0 && acks[send_base]) {
            uint64_t tmp = send_base;
            send_base += packets[send_base].GetDataLength();
            acks[tmp] = false;
            packets.erase(tmp);
        }
    }
}

bool RDTConnection::connected() const { return is_connected; }

bool RDTConnection::ConfigureTimeout(int sec, int usec) {
    struct timeval t_val;
    t_val.tv_sec = sec;
    t_val.tv_usec = usec;

    auto rc = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &t_val,
                          sizeof(t_val));
    if (rc != 0) { PrintErrorAndDC("Setting timeout value"); }
    return (rc == 0);
}

void RDTConnection::PrintErrorAndDC(const string& msg) {
    fprintf(stderr, "ERROR: %s.\n", msg.c_str());
    is_connected = false;
}

void RDTConnection::PrintPacketInfo(const Packet& packet, rec_or_sender_t rs,
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
    if (packet.GetPacketType() != Packet::NONE) {
        cout << " " << packet.TypeToString();
    }
    cout << endl;
}

uint64_t RDTConnection::CalculateOffset(uint64_t num) {
    int rem = num % constants::MAX_SEQ_NUM;
    if (rem != 0) { return num - rem; }
    return num;
}
