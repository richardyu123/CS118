#ifndef RDT_CONNECTION_H
#define RDT_CONNECTION_H

#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

#include "Packet.h"

class RDTConnection {
public:
    RDTConnection(const int sock_fd);
    ~RDTConnection();

    RDTConnection& SendMessage(const std::string& input);
    void Read(std::string& str, size_t num_bytes);

    bool connected() const;
protected:
    bool ConfigureTimeout(int sec, int usec);
    void PrintErrorAndDC(const std::string& msg);
    Packet* front_packet;

    socklen_t cli_len;
    struct sockaddr_in cli_addr;
    const int sock_fd;
    bool is_connected;
    uint16_t next_seq_num;
    uint16_t send_base;
    uint16_t receive_base;
    virtual void Handshake() = 0;
    virtual void Finish() = 0;
};

#endif
