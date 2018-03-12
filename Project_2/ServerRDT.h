#ifndef SERVER_RDT_H
#define SERVER_RDT_H

#include "Packet.h"
#include "RDTConnection.h"

class ServerRDT : public RDTConnection {
public:
    ServerRDT(const int sock_fd);
    ~ServerRDT();

protected:
    virtual void SendPacket(const Packet& packet);
    virtual void Handshake();
    virtual void Finish();
};

#endif
