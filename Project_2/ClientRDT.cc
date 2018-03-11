#include <iostream>
#include <string>


#include "ClientRDT.h"
#include "Constants.h"
#include "Packet.h"

ClientRDT::ClientRDT(const int sock_fd) : RDTConnection(sock_fd) {
    Handshake();
}
ClientRDT::~ClientRDT() {}
void ClientRDT::Handshake() {
    char buf[constants::MAX_PACKET_LEN];
    next_seq_num = 0;
    Packet packet = Packet(Packet::SYN, next_seq_num, constants::WINDOW_SIZE, nullptr, 0);
    send_base = 0;
    if(!ConfigureTimeout(0, constants::RETRANS_TIMEOUT_us)) {
        return;
    }
    
    while (true) {
        std::cout << "Sending packet of type: " << packet.TypeToString() <<
            endl;
        write(sock_fd, packet.GetPacketData().c_str(), packet.GetPacketLength());
        
        ssize_t num_bytes = recv(sock_fd, buf, constants::MAX_PACKET_LEN, 0);
        if (num_bytes <= 0) {
            continue;
        }
        
        Packet packet = Packet(buf, num_bytes);
        std::cout << "Received packet of type: " << packet.TypeToString() << endl;
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
    write(sock_fd, packet2.GetPacketData().c_str(), packet2.GetPacketLength());
}

void ClientRDT::Finish() {
}
