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

    typedef struct PacketAndSeq {
        PacketAndSeq(Packet pkt, uint64_t num) : pkt(pkt), num(num) {}
        Packet pkt;
        uint64_t num;
    } packet_seq_t;

    RDTConnection& SendMessage(const std::string& input);
    void Read(std::basic_ostream<char>& os, size_t num_bytes);
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
    uint16_t next_seq_num;
    uint16_t send_base;
    uint16_t receive_base;
    virtual void SendPacket(const Packet& packet) = 0;
    virtual void Handshake() = 0;
    virtual void Finish() = 0;
private:
    std::list<packet_seq_t> received;
    uint64_t Floor(uint64_t num);
};

#endif
