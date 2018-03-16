#ifndef CLIENT_RDT_H
#define CLIENT_RDT_H

#include "RDTController.h"

class ClientRDT : public RDTController {
public:
    ClientRDT(const int sock_fd);
    ~ClientRDT();
    
protected:
    virtual ssize_t ReceivePacket(Packet& packet);
    virtual void SendPacket(const Packet& packet, bool retrans);
    virtual void Handshake();
    virtual void Close();
};

#endif
