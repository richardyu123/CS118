#include <iostream>
#include <string>

#include "ClientRDT.h"
#include "Packet.h"
#include "Parameters.h"

using namespace std;

ClientRDT::ClientRDT(const int sock_fd) : RDTController(sock_fd) {
    Handshake();
}

ClientRDT::~ClientRDT() {
    if (is_connected) { Close(); }
}

ssize_t ClientRDT::ReceivePacket(Packet& packet) {
    char buffer[parameters::MAX_PACKET_LEN];
    auto num_bytes = read(sock_fd, buffer, parameters::MAX_PACKET_LEN);
    if (num_bytes > 0) {
        packet = Packet(buffer, num_bytes);
    }
    PrintPacketInfo(packet, RECEIVER, false);
    return num_bytes;
}

void ClientRDT::SendPacket(const Packet& packet, bool retrans) {
    PrintPacketInfo(packet, SENDER, retrans);
    write(sock_fd, packet.GetPacketData().data(), packet.GetPacketLength());
}

// Send SYN, expect SYNACK, send ACK.
void ClientRDT::Handshake() {
    next_seq_num = 0;
    Packet packet = Packet(Packet::SYN, next_seq_num, parameters::WINDOW_SIZE,
                           nullptr, 0);
    send_base = 0;
    bool retrans = false;

    if(!ConfigureTimeout(0, parameters::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    while (true) {
        SendPacket(packet, retrans); 
        
        Packet packet;
        auto num_bytes = ReceivePacket(packet);
        if (num_bytes <= 0) {
            retrans = true;
            continue;
        }
        
        if (packet.GetType() == Packet::SYNACK) {
            receive_base = packet.GetPacketNumber() + 1;
            break;
        }
        else {
            PrintErrorAndDC("Expected SYNACK");
        }
    }
    
    send_base++;
    next_seq_num++;
    Packet packet2 = Packet(Packet::ACK, next_seq_num, parameters::WINDOW_SIZE,
                            nullptr, 0);
    SendPacket(packet2, false);
}

void ClientRDT::Close() {
    ssize_t num_bytes;

    if (!ConfigureTimeout(0, 0)) { return; }

    while (true) {
        Packet pkt;
        num_bytes = ReceivePacket(pkt);
        if (num_bytes <= 0) {
            cerr << "Error on receiving." << endl;
            return;
        } else {
            if (pkt.GetType() == Packet::FIN) { break; }
            // Unexpected packet type.
            Packet pkt2(Packet::ACK, pkt.GetPacketNumber(),
                        parameters::WINDOW_SIZE, nullptr, 0);
            SendPacket(pkt2, false);
        }
    }

    Packet pkt(Packet::ACK, receive_base, parameters::WINDOW_SIZE, nullptr, 0);
    SendPacket(pkt, false);

    receive_base++;
    pkt = Packet(Packet::FIN, next_seq_num, parameters::WINDOW_SIZE, nullptr,
                 0);
    next_seq_num++;
    bool retrans = false;

    if (!ConfigureTimeout(0, parameters::RETRANS_TIMEOUT_us)) { return; }

    while (true) {
        SendPacket(pkt, retrans);

        Packet pkt2;
        num_bytes = ReceivePacket(pkt2);
        if (num_bytes < 0) {
            retrans = true;
            continue;
        } else if (num_bytes == 0) {
            break;
        } else {
            if (pkt2.GetType() == Packet::FIN) {
                retrans = true;
                continue;
            } else if (pkt2.GetType() == Packet::ACK) {
                break;
            } else {
                // Unexpected packet type.
                continue;
            }
        }
    }
    send_base++;
}
