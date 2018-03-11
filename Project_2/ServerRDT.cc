#include <chrono>
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
            PrintPacketInfo(pkt, RECEIVER, false);
            if (pkt.GetPacketType() == Packet::SYN) {
                receive_base = pkt.GetPacketNumber() + 1;
                break;
            } else if (pkt.GetPacketType() == Packet::FIN) {
                // Unexpected FIN.
                pkt = Packet(Packet::ACK, pkt.GetPacketNumber(),
                            constants::WINDOW_SIZE, nullptr, 0);
                PrintPacketInfo(pkt, SENDER, false);
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
        PrintPacketInfo(pkt, SENDER, retrans);
        sendto(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength(), 0,
               (struct sockaddr*)&cli_addr, cli_len);
        len = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN, 0,
                       (struct sockaddr*)&cli_addr, &cli_len);
        if (len > 0) {
            pkt = Packet(buffer, len);
            PrintPacketInfo(pkt, RECEIVER, false);
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
    ssize_t num_bytes;
    char buffer[constants::MAX_PACKET_LEN];

    bool retrans = false;
    Packet pkt(Packet::FIN, next_seq_num, constants::WINDOW_SIZE, nullptr, 0);
    next_seq_num++;
    bool waiting_for_fin = true;
    bool received_fin = false;

    if(!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) { return; }

    while (true) {
        PrintPacketInfo(pkt, SENDER, retrans);
        retrans = true;
        sendto(sock_fd, pkt.GetPacketData().data(), pkt.GetPacketLength(), 0,
               (struct sockaddr*)&cli_addr, cli_len);

        num_bytes = recv(sock_fd, buffer, constants::MAX_PACKET_LEN, 0);
        if (num_bytes <= 0) { continue; }

        Packet pkt2(buffer, num_bytes);
        PrintPacketInfo(pkt2, RECEIVER, false);

        if (pkt2.GetPacketType() == Packet::ACK) {
            if (pkt2.GetPacketNumber() == pkt.GetPacketNumber()) {
                break;
            } else {
                continue;
            }
        } else if (pkt2.GetPacketType() == Packet::FIN) {
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
            num_bytes = recvfrom(sock_fd, buffer, constants::MAX_PACKET_LEN,
                                 0, (struct sockaddr*)&cli_addr, &cli_len);
            if (num_bytes <= 0) {
                if (!received_fin) {
                    cout << "Did not receive FIN during window." << endl;
                }
                break;
            }

            Packet pkt(buffer, num_bytes);
            PrintPacketInfo(pkt, RECEIVER, false);
            if (pkt.GetPacketType() == Packet::FIN) {
                receive_base = pkt.GetPacketNumber();
                received_fin = true;
            } else {
                // Unexpected packet type.
                continue;
            }
        }
        Packet pkt2(Packet::ACK, receive_base, constants::WINDOW_SIZE,
                    nullptr, 0);
        PrintPacketInfo(pkt2, SENDER, retrans);
        sendto(sock_fd, pkt2.GetPacketData().data(), pkt2.GetPacketLength(),
                0, (struct sockaddr*)&cli_addr, cli_len);
        waiting_for_fin = true;
    }

    receive_base++;
    return;
}
