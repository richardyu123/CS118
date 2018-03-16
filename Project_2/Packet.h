#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <string>
#include <unistd.h>
#include <vector>

class Packet {
public:
    typedef enum PacketType {
        SYN = 0b00000001,
        ACK = 0b00000010,
        FIN = 0b00000100,
        DATA = 0b000000000,
        SYNACK = 0b00000011
    } packet_t;

    Packet(packet_t packet_type, uint16_t packet_num, uint16_t window_size,
           char* data, size_t data_length);
    Packet(char* full_data, size_t data_length);
    Packet();

    std::string TypeToString() const;

    // Getters.
    packet_t GetType() const;
    uint16_t GetPacketNumber() const;
    uint16_t GetWindowSize() const;
    bool isValid() const;
    std::vector<char> GetData() const;
    const std::vector<char>& GetPacketData() const;
    size_t GetDataLength() const;
    size_t GetPacketLength() const;
private:
    void FillWithFullData();
    void FillHeader();

    packet_t packet_type;
    uint16_t packet_num;
    uint16_t window_size;
    std::vector<char> packet_data;
    size_t data_length;
    bool valid;
};

#endif
