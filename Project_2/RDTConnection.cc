#include <chrono>
#include <iostream>
#include <list>
#include <stdlib.h>
#include <unordered_map>

#include "Constants.h"

#include "RDTConnection.h"

#include <stdio.h>

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

void RDTConnection::Read(string& str, size_t count) {
    str.resize(count);
    auto str_iter = str.begin();
    size_t curr = 0;

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

        ssize_t len;
        char buffer[constants::MAX_PACKET_LEN];

        if (!ConfigureTimeout(0, 0)) { return; }
        while (curr < count) {
            Packet pkt;
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

            uint64_t seq_num;
            uint32_t seq = receive_base % constants::MAX_SEQ_NUM;
            if (seq + constants::WINDOW_SIZE > constants::MAX_SEQ_NUM) {
                if (pkt.GetPacketNumber() < seq && pkt.GetPacketNumber() <= 
                        constants::WINDOW_SIZE) {
                    seq_num = pkt.GetPacketNumber() + receive_base +
                        constants::MAX_SEQ_NUM - seq;
                }
            } else {
                if (receive_base % constants::MAX_SEQ_NUM == 0) {
                    seq_num = pkt.GetPacketNumber() + receive_base;
                } else {
                    seq_num = pkt.GetPacketNumber() + receive_base -
                        (receive_base % constants::MAX_SEQ_NUM);
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
                } else {
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
                    copy(data.begin(), data.end() + bytes, str_iter);
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
}

void RDTConnection::Write() {
    string filename = "hello.txt";
    ifstream inFile;
    inFile.open(filename)
    
    if(!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    // Data structures:
    unordered_map<uint64_t, Packet> packets; // Seq_num -> packet.
    unordered_map<uint64_t, milliseconds> timestamps; // Seq_num -> timestamp.
    unordered_map<uint64_t, bool> acks; // Seq_num -> acked.
    list<uint64_t> packet_list; // List of packets' seq_nums.
    
    while (true) {

        // Step 2
        bool done_sending = false;
        while (next_seq_num < send_base + constants::WINDOW_SIZE) {
            size_t data_size = min(constants::MAX_PACKET_LEN - constants::HEADER_SIZE, send_base + constants::WINDOW_SIZE - next_seq_num);
            if (data_size == 0) {
                done_sending = true;
                break;
            }
            char buf[data_size];
            if (inFile.is_open()) {
                inFile.read(buf, data_size);
            }
            
            Packet pkt = Packet(Packet::NONE, next_seq_num % constants::WINDOW_SIZE, buf, data_size);
            
            // TODO: update data structures
            
            SendPacket();
            next_seq_num += data_size;
        }
        
        if (done_sending) {
            break;
        }
        
        // Step 4
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
