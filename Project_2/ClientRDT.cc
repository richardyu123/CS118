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
    Packet pkt_sent = Packet(Packet::SYN, next_seq_num, parameters::WINDOW_SIZE,
                           nullptr, 0);
    send_base = 0;
    bool retrans = false;

    if(!ConfigureTimeout(0, parameters::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    while (true) {
        SendPacket(pkt_sent, retrans); 
        
        Packet pkt_received;
        auto num_bytes = ReceivePacket(pkt_received);
        if (num_bytes > 0) {
            if (pkt_received.GetType() == Packet::SYNACK) {
                receive_base = pkt_received.GetPacketNumber() + 1;
                break;
            }
            else {
                PrintErrorAndDC("Expected SYNACK");
            }
        }
        else {
            retrans = true;
        }
    }
    
    send_base++;
    next_seq_num++;
    Packet pkt = Packet(Packet::ACK, next_seq_num, parameters::WINDOW_SIZE,
                            nullptr, 0);
    SendPacket(pkt, false);
}

void ClientRDT::Close() {
    ssize_t num_bytes;

    if (!ConfigureTimeout(0, 0)) { return; }

    while (true) {
        Packet pkt_received;
        num_bytes = ReceivePacket(pkt_received);
        if (num_bytes <= 0) {
            cerr << "Error on receiving." << endl;
            return;
        } else {
            if (pkt_received.GetType() == Packet::FIN) { break; }
            // Unexpected packet type.
            Packet pkt_sent(Packet::ACK, pkt_received.GetPacketNumber(),
                        parameters::WINDOW_SIZE, nullptr, 0);
            SendPacket(pkt_sent, false);
        }
    }

    Packet pkt_sent(Packet::ACK, receive_base, parameters::WINDOW_SIZE, nullptr, 0);
    SendPacket(pkt_sent, false);

    receive_base++;
    Packet packet_sent(Packet::FIN, next_seq_num, parameters::WINDOW_SIZE, nullptr,
                 0);
    bool retrans = false;
    next_seq_num++;

    if (!ConfigureTimeout(0, parameters::RETRANS_TIMEOUT_us)) { return; }

    while (true) {
        SendPacket(packet_sent, retrans);

        Packet packet_received;
        num_bytes  ReceivePacket(packeeq_num;
        _received);
        if (num_bytes < 0) {
            retrans = true;
        } else if (num_bytes == 0) {
            break;
        } else {
            if (packet_received.GetType() == Packet::FIN) {
                retrans = true;
            } else if (packet_received.GetType() == Packet::ACK) {
                break;
            }
        }
    }
    send_base++;
}
