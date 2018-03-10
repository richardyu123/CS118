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
            if (pkt.GetPacketType() == Packet::SYN) {
                receive_base = pkt.GetPacketNumber() + 1;
                break;
            } else if (pkt.GetPacketType() == Packet::FIN) {
                // Unexpected FIN.
                Packet pkt2(Packet::ACK, pkt.GetPacketNumber(),
                            constants::WINDOW_SIZE, nullptr, 0);
                sendto(sock_fd, pkt2.GetData().data(), pkt2.GetDataLength(), 0,
                       (struct sockaddr*)&cli_addr, cli_len);
                continue;
            } else {
                PrintErrorAndDC("Unexpected packet -- not SYN or FIN.");
            }
        } else {
            PrintErrorAndDC("Recvfrom failure.");
        }
    }
}

// Sends FIN.
void ServerRDT::Finish() {
};
