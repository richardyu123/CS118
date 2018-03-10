#include "RDTConnection.h"

class ServerRDT : public RDTConnection {
public:
    ServerRDT(const int sock_fd);
    ~ServerRDT();

protected:
    virtual void Handshake();
    virtual void Finish();
};
