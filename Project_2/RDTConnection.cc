#include <chrono>
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
using namespace util;
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

//TODO: Add originality.
#include <iterator>
void RDTConnection::Read(std::basic_ostream<char>& os, size_t count) {
//    str.resize(count);
//    auto str_iter = str.begin();
    ostream_iterator<char> str_iter(os);
    size_t curr = 0;
    // TODO: INPUT INTO RECEIVED.
    while(!received.empty() && receive_base == received.front().num) {
        auto data = received.front().pkt.GetData();
        size_t bytes = min(count - curr, static_cast<size_t>(distance(
                        data.begin() + offset, data.end())));
        copy(data.begin() + offset, data.begin() + offset + bytes, str_iter);
        curr += bytes;

        if (bytes + offset == data.length()) {
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

    if (!ConfigureTimeout(0, 0)) { return; }
    while (curr < count) {
        memset(buffer, 0, constants::MAX_PACKET_LEN);
        Packet pkt;
        if (front_packet) {
            pkt = *front_packet;
            delete front_packet;
            front_packet = nullptr;
        } else {
            len = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN,
                            0, (struct sockaddr*)&cli_addr, &cli_len);
            pkt = Packet(buffer, len);
            cout << pkt.GetData() << endl;
            PrintPacketInfo(pkt, RECEIVER, false);
        }
        if (pkt.GetPacketType() != Packet::NONE) { continue; }

        uint64_t seq_num = pkt.GetPacketNumber() + Floor(receive_base);
        uint32_t seq = receive_base % constants::MAX_SEQ_NUM;
        if (seq + constants::WINDOW_SIZE > constants::MAX_SEQ_NUM) {
            if (pkt.GetPacketNumber() < seq && pkt.GetPacketNumber() <= 
                    constants::WINDOW_SIZE) {
                seq_num = pkt.GetPacketNumber() + receive_base +
                    constants::MAX_SEQ_NUM - seq;
            }
        }
        if (seq_num >= receive_base && seq_num <= receive_base +
                constants::WINDOW_SIZE - 1) {
            bool retrans = false;
            auto iter = received.begin();
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

            Packet acket(Packet::ACK, pkt.GetPacketNumber(),
                            constants::WINDOW_SIZE, nullptr, 0);
            PrintPacketInfo(acket, SENDER, retrans);
            sendto(sock_fd, acket.GetPacketData().data(),
                    acket.GetPacketLength(), 0, (struct sockaddr*)&cli_addr,
                    cli_len);

            while(receive_base == received.front().num) {
                auto data = received.front().pkt.GetData();
                size_t bytes = min(static_cast<size_t>(distance(
                                    data.begin(), data.end())), count -
                                    curr);
                for (auto iter = data.begin(); iter != data.begin() + bytes; iter++) {
                    os << *iter;
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
            Packet acket(Packet::ACK, pkt.GetPacketNumber(),
                            constants::WINDOW_SIZE, nullptr, 0);
            PrintPacketInfo(acket, SENDER, (seq_num < receive_base));
            sendto(sock_fd, acket.GetPacketData().data(),
                    acket.GetPacketLength(), 0, (struct sockaddr*)&cli_addr,
                    cli_len);
        }
    }
}

void RDTConnection::Write(string data) {
    auto begin = data.begin();
    auto end = data.end();
    if (!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    // Data structures:
    unordered_map<uint64_t, Packet> packets; // Seq_num -> packet.
    unordered_map<uint64_t, milliseconds> timestamps; // Seq_num -> timestamp.
    unordered_map<uint64_t, bool> acks; // Seq_num -> acked.
    list<uint64_t> packet_list; // List of packets' seq_nums.
    
    while (true) {
        // Step 1 
        milliseconds cur_time;
        for (auto seq_n : packet_list) {
            cur_time = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch());
            if (abs(duration_cast<milliseconds>(
                            cur_time - timestamps[seq_n]).count()) >=
                static_cast<int>(constants::RETRANS_TIMEOUT)) {
                PrintPacketInfo(packets[seq_n], SENDER, true);
                timestamps[seq_n] = duration_cast<milliseconds>(
                        system_clock::now().time_since_epoch());
                SendPacket(packets[seq_n]);
            }
        }

        // Step 2
        bool done_sending = false;
        while (next_seq_num < send_base + constants::WINDOW_SIZE) {
            size_t data_size = min(constants::MAX_PACKET_LEN - constants::HEADER_SIZE, send_base + constants::WINDOW_SIZE - next_seq_num);
            if (data_size <= 0) {
                done_sending = true;
                break;
            }
            char buf[data_size];
            memset((void*)buf, 0, data_size);
            size_t count = 0;
            while (begin != end && count < data_size) {
                buf[count] = *begin;
                begin++;
                count++;
            }
            if (count == 0) {
                done_sending = true;
                break;
            }
            Packet pkt = Packet(Packet::NONE, next_seq_num % constants::MAX_SEQ_NUM,
                                constants::WINDOW_SIZE, buf, data_size);
            
            packet_list.push_back(next_seq_num);
            packets[next_seq_num] = pkt;
            
            auto duration = system_clock::now().time_since_epoch();
            auto timestamp = duration_cast<milliseconds>(duration);
            timestamps[next_seq_num] = timestamp;
            
            PrintPacketInfo(pkt, SENDER, false);
            char test[pkt.GetPacketLength()];
            for (int i = 0; i < pkt.GetPacketLength(); i++) {
                test[i] = pkt.GetPacketData()[i];
            }
            Packet pkt_test(test, pkt.GetPacketLength());
            cout << "Sending: " << pkt_test.GetData() << endl;
            SendPacket(pkt);
            next_seq_num += data_size;
        }
        
        if (done_sending && packets.empty()) {
            break;
        }
        
        // Step 3
        char buffer[constants::MAX_PACKET_LEN];
        memset(buffer, 0, constants::MAX_PACKET_LEN);
        auto num_bytes = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN,
                                  0, (struct sockaddr*)&cli_addr, &cli_len);

        if (num_bytes > 0) {
            Packet pkt(buffer, num_bytes);

            if (pkt.GetPacketType() == Packet::ACK) {
                auto ack_num = pkt.GetPacketNumber() + Floor(send_base);
                auto curr_seq = send_base % constants::MAX_SEQ_NUM;
                if (curr_seq + constants::WINDOW_SIZE >
                        constants::MAX_SEQ_NUM) {
                    if (pkt.GetPacketNumber() < curr_seq &&
                            pkt.GetPacketNumber() <= constants::WINDOW_SIZE) {
                        ack_num = pkt.GetPacketNumber() + send_base +
                            constants::MAX_SEQ_NUM - curr_seq;
                    }
                }

                if (send_base <= ack_num && ack_num <= send_base +
                        constants::WINDOW_SIZE) {
                    PrintPacketInfo(pkt, RECEIVER, false);
                    acks[ack_num] = true;
                    // NB: Might cause error if two of the same ack_nums are in the list.
                    packet_list.remove(ack_num);
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

        // Step 4
        while (acks.count(send_base) != 0 && acks[send_base]) {
            uint16_t tmp = send_base;
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

uint64_t RDTConnection::Floor(uint64_t num) {
    auto rem = num % constants::MAX_SEQ_NUM;
    if (rem) { return num - rem; }
    return num;
}
