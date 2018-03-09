#include "Packet.h"

Packet::Packet(packet_t packet_type, uint16_t packet_num, uint16_t window_num,
               char* data, size_t data_length)
    : packet_type(packet_type), packet_num(packet_num), window_num(window_num),
      data_length(data_length) {
    packet_data.reserve(constants::HEADER_SIZE + data_length);
    FillHeader();
    if (data_length > 0) {
    }
}
