#include "Parameters.h"

#include "Packet.h"

using namespace std;

Packet::Packet(packet_t packet_type, uint16_t packet_num, uint16_t window_size,
               char* data, size_t data_length)
    : packet_type(packet_type), packet_num(packet_num), window_size(window_size),
      data_length(data_length), valid(true) {
    packet_data.resize(parameters::HEADER_SIZE + data_length);
    FillHeader();
    if (data_length != 0) {
        packet_data.insert(packet_data.begin() + parameters::HEADER_SIZE, data,
                           data + data_length);
    }
}

Packet::Packet(char* full_data, size_t data_length)
    : packet_data(full_data, full_data + data_length), valid(true) {
    FillWithFullData();
}

Packet::Packet() : valid(false) {}

std::string Packet::TypeToString() const {
    switch(packet_type) {
    case SYN:
        return "SYN";
    case ACK:
        return "ACK";
    case FIN:
        return "FIN";
    case SYNACK:
        return "SYNACK";
    default:
        return "";
    }
}

Packet::packet_t Packet::GetPacketType() const { return packet_type; }

uint16_t Packet::GetPacketNumber() const { return packet_num; }

uint16_t Packet::GetWindowSize() const { return window_size; }

bool Packet::isValid() const { return valid; }

std::vector<char> Packet::GetData() const {
    return vector<char>(packet_data.begin() + parameters::HEADER_SIZE,
                        packet_data.end());
}

const std::vector<char>& Packet::GetPacketData() const {  return packet_data; }

size_t Packet::GetDataLength() const { return data_length; }

size_t Packet::GetPacketLength() const {
    return data_length + parameters::HEADER_SIZE;
}

void Packet::FillWithFullData() {
    data_length = packet_data.size() - parameters::HEADER_SIZE;
    packet_num = static_cast<uint16_t>((unsigned char)packet_data[0] << 8);
    packet_num |= static_cast<uint16_t>((unsigned char)packet_data[1]);
    packet_type = static_cast<Packet::packet_t>((unsigned char)packet_data[2]);
    window_size = static_cast<uint16_t>((unsigned char)packet_data[3] << 8);
    window_size |= static_cast<uint16_t>((unsigned char)packet_data[4]);
}

void Packet::FillHeader() {
    packet_data[0] = static_cast<char>(packet_num >> 8);
    packet_data[1] = static_cast<char>(packet_num);
    packet_data[2] = static_cast<char>(packet_type);
    packet_data[3] = static_cast<char>(window_size >> 8);
    packet_data[4] = static_cast<char>(window_size);
}
