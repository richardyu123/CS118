

class RDTConnection {
private:
public:
    typedef enum ApplicationType {
        SERVER,
        CLIENT
    } application_t;
    RDTConnection(application_t app_type, const int sock_fd);
};
