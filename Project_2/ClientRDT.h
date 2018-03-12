#ifndef CLIENT_RDT_H
#define CLIENT_RDT_H

#include "RDTConnection.h"

class ClientRDT : public RDTConnection {
public:
    ClientRDT(const int sock_fd);
    ~ClientRDT();
    
protected:
    virtual void SendPacket(const Packet& packet);
    virtual void Handshake();
    virtual void Finish();
};

#endif
