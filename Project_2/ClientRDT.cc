#include <string>
#include "Constants.h"
#include "Packet.h"

ClientRDT::ClientRDT(const int sock_fd) : RDTConnection(sock_fd) {}
ClientRDT::~ClientRDT() {}
void ClientRDT::Handshake() {
    char buf[constants::MAX_PACKET_LEN];
    
    int packet_num = 0;
    Packet packet = Packet(PacketType::SYN, packet_num, constants::WINDOW_SIZE, nullptr, 0);
    
    bool resend = false;
    
    while (true) {
        write(sock_fd, packet.GetPacketData(), packet.GetPacketLength());
        
        bytes = recv(sock_fd, buf, constants::MAX_PACKET_LEN, 0);
        if (bytes <= 0) {
            resend = true;
            continue;
        }
    }
}

void ClientRDT::Finish() {
}