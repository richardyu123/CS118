#include <string>

using namespace std;

class Packet {
public:
    typedef enum PacketType {
        SYN = 0b10000000,
        ACK = 0b01000000,
        FIN = 0b00100000,
        NONE = 0b000000000,
        SYNACK = 0b11000000
    } packet_t;

    Packet(packet_t packet_type, uint16_t packet_num, uint16_t window_num,
           char* data, size_t data_length);
    Packet();
    
private:
    string packet_data;
    size_t data_length;
    packet_t packet_type;
    uint16_t packet_num;
    uint16_t window_num;
    bool valid;
};
