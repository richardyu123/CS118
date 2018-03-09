#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

using namespace std;

class RDTConnection {
public:
    RDTConnection(const int sock_fd);
    ~RDTConnection();

    RDTConnection& SendMessage(const string& input);
    void Read(const string& str, size_t num_bytes);

    bool connected() const;
protected:
    socklen_t cli_len;
    struct sockaddr_in cli_addr;
    const int sock_fd;
    bool is_connected;
    virtual void Handshake() = 0;
    virtual void Finish() = 0;
};
