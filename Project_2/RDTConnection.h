#include <netinet/in.h>
#include <sys/socket.h>

class RDTConnection {
public:
    typedef enum ApplicationType {
        SERVER,
        CLIENT
    } application_t;

    RDTConnection(application_t app_type, const int sock_fd);
    ~RDTConnection();

    RDTConnection& SendMessage(const string& input);
    void Write();
    void Read(const string& str, size_t num_bytes);

    bool connected() const;
private:
    const int sock_fd;
    socklen_t cli_len;
    application_t app_type;
    struct sockaddr_in cli_addr;
    bool connected;

    void AckHandshake();
    void InitiateHandshake();

    void AckFinish();
    void Finish();
};
