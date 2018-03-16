#include <chrono>
#include <iostream>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Packet.h"
#include "Parameters.h"

#include "ServerRDT.h"

using namespace std;

ServerRDT::ServerRDT(const int sock_fd)
    : RDTController(sock_fd) {
    Handshake();
}

ServerRDT::~ServerRDT() {
    if (is_connected) {
        Close();
    }
}

ssize_t ServerRDT::ReceivePacket(Packet& packet) {
    char buffer[parameters::MAX_PACKET_LEN];
    auto num_bytes = recvfrom(sock_fd, buffer, parameters::MAX_PACKET_LEN, 0,
                              (struct sockaddr*)&cli_addr, &cli_len);
    if (num_bytes > 0) {
        packet = Packet(buffer, num_bytes);
        PrintPacketInfo(packet, RECEIVER, false);
    }
    return num_bytes;
}

void ServerRDT::SendPacket(const Packet& packet, bool retrans) {
    PrintPacketInfo(packet, SENDER, retrans);
    sendto(sock_fd, packet.GetPacketData().data(), packet.GetPacketLength(),
           0, (struct sockaddr*)&cli_addr, cli_len);
}

// Receives the handshake.
void ServerRDT::Handshake() {
    ssize_t num_bytes;
    bool waiting = true;

    if(!ConfigureTimeout(0, 0)) { return; }

    while (waiting) {
        // Receive packet from socket.
        Packet pkt;
        num_bytes = ReceivePacket(pkt);
        if (num_bytes <= 0) {
            PrintErrorAndDC("Recvfrom failure");
            return;
        }
        // Expecting packet of type SYN.
        switch (pkt.GetType()) {
        case Packet::SYN:
            receive_base = pkt.GetPacketNumber() + 1;
            waiting = false;
            break;
        case Packet::FIN:
            // Unexpected FIN.
            pkt = Packet(Packet::ACK, pkt.GetPacketNumber(),
                         parameters::WINDOW_SIZE, nullptr, 0);
            PrintPacketInfo(pkt, SENDER, false);
            sendto(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength(),
                   0, (struct sockaddr*)&cli_addr, cli_len);
            break;
        default:
            PrintErrorAndDC("Unexpected packet -- not SYN or FIN");
            return;
        }
    }
    if (!ConfigureTimeout(0, parameters::RETRANS_TIMEOUT_us)) { return; }

    // Generate and send SYNACK packet.
    Packet pkt(Packet::SYNACK, next_seq_num, parameters::WINDOW_SIZE, nullptr,
               0);
    waiting = true;
    bool retrans = false;

    while (waiting) {
        PrintPacketInfo(pkt, SENDER, retrans);
        sendto(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength(), 0,
               (struct sockaddr*)&cli_addr, cli_len);
        // Expecting packet of type ACK.
        Packet pkt2;
        num_bytes = ReceivePacket(pkt2);
        if (num_bytes <= 0) {
            retrans = true;
            continue;
        }
        switch (pkt2.GetType()) {
        case Packet::SYN:
            // We received a duplicate SYN.
            retrans = true;
            break;
        case Packet::ACK:
            waiting = false;
            break;
        default:
            front_packet = new Packet(pkt);
            waiting = false;
            break;
        }
    }

    send_base++;
    next_seq_num++;
}

// Sends FIN.
void ServerRDT::Close() {
    ssize_t num_bytes;

    bool retrans = false;
    Packet pkt(Packet::FIN, next_seq_num, parameters::WINDOW_SIZE, nullptr, 0);
    next_seq_num++;
    bool waiting_for_fin = true;
    bool received_fin = false;

    if(!ConfigureTimeout(0, parameters::RETRANS_TIMEOUT_us)) { return; }

    // Send FIN, expect ACK.
    while (true) {
        SendPacket(pkt, retrans);
        retrans = true;

        Packet pkt2;
        num_bytes = ReceivePacket(pkt2);
        if (num_bytes <= 0) { continue; }

        if (pkt2.GetType() == Packet::ACK) {
            if (pkt2.GetPacketNumber() == pkt.GetPacketNumber()) {
                break;
            } else {
                continue;
            }
        } else if (pkt2.GetType() == Packet::FIN) {
            receive_base = pkt2.GetPacketNumber();
            waiting_for_fin = false;
            received_fin = true;
            break;
        } else {
            // Unexpected packet.
            continue;
        }
    }

    send_base++;
    chrono::seconds start = chrono::duration_cast<chrono::seconds>(
            chrono::system_clock::now().time_since_epoch());
    // Expect FIN, send ACK.
    while (true) {
        if (waiting_for_fin) {
            chrono::seconds cur_time = chrono::duration_cast<chrono::seconds>(
                    chrono::system_clock::now().time_since_epoch());
            auto elapsed = cur_time - start;
            if (chrono::duration_cast<chrono::seconds>(elapsed).count() >= 1) {
                // Timed wait finished.
                break;
            }

            auto remaining = (chrono::seconds)(1) - elapsed;
            auto remaining_us = remaining -
                chrono::duration_cast<chrono::seconds>(remaining);
            ConfigureTimeout(chrono::duration_cast<chrono::seconds>(
                             remaining).count(),
                             chrono::duration_cast<chrono::microseconds>(
                             remaining_us).count());
            Packet pkt;
            num_bytes = ReceivePacket(pkt);
            if (num_bytes <= 0) {
                if (!received_fin) {
                    cout << "Did not receive FIN during window." << endl;
                }
                break;
            }

            if (pkt.GetType() == Packet::FIN) {
                receive_base = pkt.GetPacketNumber();
                received_fin = true;
            } else {
                // Unexpected packet type.
                continue;
            }
        }
        Packet pkt2(Packet::ACK, receive_base, parameters::WINDOW_SIZE,
                    nullptr, 0);
        SendPacket(pkt2, false);
        waiting_for_fin = true;
    }

    receive_base++;
}
