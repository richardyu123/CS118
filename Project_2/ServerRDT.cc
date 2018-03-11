#include <iostream>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Constants.h"
#include "Packet.h"

#include "ServerRDT.h"

using namespace std;

ServerRDT::ServerRDT(const int sock_fd)
    : RDTConnection(sock_fd) {
    Handshake();
}

ServerRDT::~ServerRDT() {
    if (is_connected) {
        Finish();
    }
}

void ServerRDT::SendPacket(Packet packet) {
    sendto(sock_fd, packet.GetPacketData().c_str(), packet.getPacketLength(), 0, (struct sockaddr*)&cli_addr, cli_len);
}

// Receives the handshake.
void ServerRDT::Handshake() {
    ssize_t len;
    char buffer[constants::MAX_PACKET_LEN];

    if(!ConfigureTimeout(0, 0)) { return; }

    while (true) {
        len = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN, 0,
                       (struct sockaddr*)&cli_addr, &cli_len);
        if (len > 0) {
            Packet pkt(buffer, len);
            if (pkt.GetPacketType() == Packet::SYN) {
                receive_base = pkt.GetPacketNumber() + 1;
                break;
            } else if (pkt.GetPacketType() == Packet::FIN) {
                // Unexpected FIN.
                pkt = Packet(Packet::ACK, pkt.GetPacketNumber(),
                            constants::WINDOW_SIZE, nullptr, 0);
                sendto(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength(), 0,
                       (struct sockaddr*)&cli_addr, cli_len);
                continue;
            } else {
                PrintErrorAndDC("Unexpected packet -- not SYN or FIN");
            }
        } else {
            PrintErrorAndDC("Recvfrom failure");
        }
    }
    if (!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) { return; }

    Packet pkt(Packet::SYNACK, next_seq_num, constants::WINDOW_SIZE, nullptr,
               0);
    bool retrans = false;

    while (true) {
        std::cout << "Sending packet of type: " << pkt.TypeToString() << endl;
        len = sendto(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength(), 0,
               (struct sockaddr*)&cli_addr, cli_len);
        len = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN, 0,
                       (struct sockaddr*)&cli_addr, &cli_len);
        if (len > 0) {
            pkt = Packet(buffer, len);
            std::cout << "Received packet of type: " << pkt.TypeToString() <<
                endl;
            if (pkt.GetPacketType() == Packet::SYN) {
                retrans = true;
                continue;
            } else if (pkt.GetPacketType() != Packet::ACK) {
                front_packet = new Packet(pkt);
            }
            break;
        } else {
            retrans = true;
        }
    }

    send_base++;
}

// Sends FIN.
void ServerRDT::Finish() {
};
