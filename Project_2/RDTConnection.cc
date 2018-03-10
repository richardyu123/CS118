#include <chrono>
#include <stdlib.h>
#include <unordered_map>

#include "Constants.h"

#include "RDTConnection.h"

using namespace std;
using namespace util;

RDTConnection::RDTConnection(const int sock_fd)
    : cli_len(sizeof(cli_addr)), sock_fd(sock_fd), is_connected(true),
      next_seq_num(0), send_base(0) {}

RDTConnection::~RDTConnection() {}

RDTConnection& RDTConnection::SendMessage(const string& input) {
    ssize_t len;
    char buffer[256];

    // Sequence to packet sizes.
    unordered_map<uint64_t, size_t> pckt_sizes;
}

bool RDTConnection::ConfigureTimeout(int sec, int usec) {
    struct timeval t_val;
    t_val.tv_sec = sec;
    t_val.tv_usec = usec;

    auto rc = setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &t_val,
                          sizeof(t_val));
    if (rc != 0) { PrintErrorAndDC("Setting timeout value."); }
    return (rc == 0);
}

void RDTConnection::PrintErrorAndDC(const string& msg) {
    fprintf(stderr, "ERROR: %s.\n", msg.c_str());
    is_connected = false;
}
