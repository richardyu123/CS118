#ifndef SERVER_RDT_H
#define SERVER_RDT_H

#include "Packet.h"
#include "RDTController.h"

class ServerRDT : public RDTController {
public:
    ServerRDT(const int sock_fd);
    ~ServerRDT();

protected:
    virtual ssize_t ReceivePacket(Packet& packet);
    virtual void SendPacket(const Packet& packet, bool retrans);
    virtual void Handshake();
    virtual void Finish();
};

#endif
