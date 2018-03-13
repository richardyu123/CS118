#include <iostream>
#include <string>

#include "ClientRDT.h"
#include "Constants.h"
#include "Packet.h"

ClientRDT::ClientRDT(const int sock_fd) : RDTConnection(sock_fd) {
    Handshake();
}

ClientRDT::~ClientRDT() {
    if (is_connected) { Finish(); }
}

void ClientRDT::SendPacket(const Packet& packet, bool retrans) {
    PrintPacketInfo(packet, SENDER, retrans);
    write(sock_fd, packet.GetPacketData().data(), packet.GetPacketLength());
}

void ClientRDT::Handshake() {
    char buf[constants::MAX_PACKET_LEN];
    next_seq_num = 0;
    Packet packet = Packet(Packet::SYN, next_seq_num, constants::WINDOW_SIZE, nullptr, 0);
    send_base = 0;
    if(!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    while (true) {
        PrintPacketInfo(packet, SENDER, false);
        write(sock_fd, packet.GetPacketData().data(), packet.GetPacketLength());
        
        ssize_t num_bytes = recv(sock_fd, buf, constants::MAX_PACKET_LEN, 0);
        if (num_bytes <= 0) {
            continue;
        }
        
        Packet packet = Packet(buf, num_bytes);
        PrintPacketInfo(packet, RECEIVER, false);
        if (packet.GetPacketType() == Packet::SYNACK) {
            receive_base = packet.GetPacketNumber() + 1;
            break;
        }
        else {
            PrintErrorAndDC("Expected SYNACK");
        }
    }
    
    send_base += 1;
    next_seq_num += 1;
    Packet packet2 = Packet(Packet::ACK, next_seq_num, constants::WINDOW_SIZE, nullptr, 0);
    fprintf(stderr, "Sending ACK.\n");
    write(sock_fd, packet2.GetPacketData().data(), packet2.GetPacketLength());
}

void ClientRDT::Finish() {
    ssize_t num_bytes;
    char buffer[constants::MAX_PACKET_LEN];

    if (!ConfigureTimeout(0, 0)) { return; }

    while (true) {
        num_bytes = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN, 0,
                             (struct sockaddr*)&cli_addr, &cli_len);
        if (num_bytes <= 0) {
            cerr << "Error on receiving." << endl;
            return;
        } else {
            Packet pkt(buffer, num_bytes);
            PrintPacketInfo(pkt, RECEIVER, false);
            
            if (pkt.GetPacketType() == Packet::FIN) { break; }
            // Unexpected packet type.
            Packet pkt2(Packet::ACK, pkt.GetPacketNumber(),
                        constants::WINDOW_SIZE, nullptr, 0);
            PrintPacketInfo(pkt, SENDER, false);
            sendto(sock_fd, pkt2.GetPacketData().data(), pkt2.GetPacketLength(),
                   0, (struct sockaddr*)&cli_addr, cli_len);
        }
    }

    Packet pkt(Packet::ACK, receive_base, constants::WINDOW_SIZE, nullptr, 0);
    PrintPacketInfo(pkt, SENDER, false);
    sendto(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength(), 0,
           (struct sockaddr*)&cli_addr, cli_len);

    receive_base++;
    pkt = Packet(Packet::FIN, next_seq_num, constants::WINDOW_SIZE, nullptr,
                 0);
    next_seq_num++;
    bool retrans = false;

    retrans = false;
    if (!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) { return; }

    while (true) {
        PrintPacketInfo(pkt, SENDER, retrans);
        write(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength());

        num_bytes = read(sock_fd, buffer, constants::MAX_PACKET_LEN);
        if (num_bytes < 0) {
            retrans = true;
            continue;
        } else if (num_bytes == 0) {
            break;
        } else {
            Packet pkt2(buffer, num_bytes);
            PrintPacketInfo(pkt2, RECEIVER, false);
            if (pkt2.GetPacketType() == Packet::FIN) {
                retrans = true;
                continue;
            } else if (pkt2.GetPacketType() == Packet::ACK) {
                break;
            } else {
                // Unexpected packet type.
                continue;
            }
        }
    }
    send_base++;
}
