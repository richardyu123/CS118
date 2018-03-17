#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <string>
#include <vector>

class Packet {
public:
    typedef enum PacketType {
        SYN,
        ACK,
        FIN,
        DATA,
        SYNACK
    } packet_t;

    Packet(packet_t packet_type, char* data, size_t data_length,
           uint16_t packet_num, uint16_t window_size);
    Packet(char* full_data, size_t data_length);
    Packet();

    std::string TypeToString() const;

    // Getters.
    packet_t GetType() const;
    uint16_t GetPacketNumber() const;
    uint16_t GetWindowSize() const;
    std::vector<char> GetData() const;
    const std::vector<char>& GetPacketData() const;
    size_t GetDataLength() const;
    size_t GetPacketLength() const;
private:
    packet_t packet_type;
    uint16_t packet_num;
    uint16_t window_size;
    std::vector<char> packet_data;
    size_t data_length;
};

#endif
