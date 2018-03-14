#ifndef RDT_CONNECTION_H
#define RDT_CONNECTION_H

#include <list>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

#include "Packet.h"

class RDTConnection {
public:
    RDTConnection(const int sock_fd);
    ~RDTConnection();

    // Send or receive data over the socket.
    void Read(std::string& str_buffer, size_t num_bytes);
    void Write(const std::string& data, uint32_t max_size = 0);

    bool connected() const;
protected:    
    typedef enum ReceiverOrSender {
        SENDER,
        RECEIVER
    } rec_or_sender_t;

    bool ConfigureTimeout(int sec, int usec);
    void PrintErrorAndDC(const std::string& msg);
    void PrintPacketInfo(const Packet& packet, rec_or_sender_t rs,
                         bool retrans);

    Packet* front_packet;

    socklen_t cli_len;
    struct sockaddr_in cli_addr;
    const int sock_fd;
    bool is_connected;
    size_t offset;
    uint64_t next_seq_num;
    uint64_t send_base;
    uint64_t receive_base;
    virtual ssize_t ReceivePacket(Packet& packet) = 0;
    virtual void SendPacket(const Packet& packet, bool retrans) = 0;
    virtual void Handshake() = 0;
    virtual void Finish() = 0;
private:
    typedef struct PacketAndSeq {
        PacketAndSeq(Packet pkt, uint64_t num) : pkt(pkt), num(num) {}
        Packet pkt;
        uint64_t num;
    } packet_seq_t;

    std::list<packet_seq_t> received;
    uint64_t CalculateOffset(uint64_t num);
};

#endif
