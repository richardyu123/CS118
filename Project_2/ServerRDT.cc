#include "Constants.h"

#include "ServerRDT.h"

using namespace std;

ServerRDT::ServerRDT(const int sock_fd)
    : RDTConnection(sock_fd) {}

ServerRDT::~ServerRDT() {}

// Receives the handshake.
void ServerRDT::Handshake() {
    ssize_t len;
    char buffer[constants::max_packet_len];
}

// Sends FIN.
void ServerRDT::Finish() {
}
