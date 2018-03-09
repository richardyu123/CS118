#include "Constants.h"

#include "ServerRDT.h"

ServerRDT::ServerRDT(const int sock_fd)
    : RDTConnection(sock_fd) {}

ServerRDT::~ServerRDT() {}

// Receives the handshake.
void ServerRDT::Handshake() {
    ssize_t len;
}

// Sends FIN.
void ServerRDT::Finish() {
}
