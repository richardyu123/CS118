#include "Constants.h"

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

    
}

// Sends FIN.
void ServerRDT::Finish() {
};
