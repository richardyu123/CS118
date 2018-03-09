#include "RDTConnection.h"

RDTConnection::RDTConnection(const int sock_fd)
    : cli_len(sizeof(cli_addr), sock_fd(sock_fd), is_connected(true) {
    Handshake();
}

RDTConnection::~RDTConnection() {
    if (is_connected) { Finish(); }
}

RDTConnection& RDTConnection::SendMessage(const string& input) {
    ssize_t len;
    char buffer[256];

    // Sequence to packet sizes.
    unordered_map<uint64_t, size_t> pckt_sizes;
}
