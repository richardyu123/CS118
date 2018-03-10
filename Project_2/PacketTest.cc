#include <cassert>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "Packet.h"

using namespace std;

int main() {
    char test_str[6] = "moose";
    uint16_t pkt_num = 0;
    uint16_t win_size = 5;
    Packet p(Packet::SYN, pkt_num, win_size, test_str, strlen(test_str));

    // Tests raw data.
    auto raw_data = p.GetData();
    auto raw_data_size = p.GetDataLength();
    assert(strcmp(raw_data.c_str(), test_str) == 0);
    assert(raw_data_size == strlen(test_str));

    // Tests input data.
    auto p_num = p.GetPacketNumber();
    auto w_size = p.GetWindowSize();
    auto p_type = p.GetPacketType();
    assert(p_num == pkt_num);
    assert(w_size == win_size);
    assert(p_type == Packet::SYN);

    // Tests header and final data.
    auto full_data = p.GetPacketData();
    auto full_data_size = p.GetPacketLength();
    auto header_p_num = static_cast<uint16_t>((unsigned char)full_data[0] << 8);
    header_p_num |= static_cast<uint16_t>((unsigned char)full_data[1]);
    auto header_p_type = static_cast<Packet::packet_t>(
            (unsigned char)full_data[2]);
    auto header_win_size = static_cast<uint16_t>(
            (unsigned char)full_data[3] << 8);
    header_win_size |= static_cast<uint16_t>((unsigned char)full_data[4]);
    assert(header_p_num == pkt_num);
    assert(header_p_type == Packet::SYN);
    assert(header_win_size == win_size);
    assert(full_data_size == (strlen(test_str) + 8));

    // Test other constructor.
    char* fd2 = new char[full_data_size];
    for (uint16_t i = 0; i < full_data_size; i++) {
        fd2[i] = full_data[i];
    }
    Packet p2(fd2, full_data_size);
    delete fd2;
    assert(full_data == p2.GetPacketData());
    assert(p.GetPacketNumber() == p2.GetPacketNumber());
    assert(p.GetWindowSize() == p2.GetWindowSize());
    assert(p.GetPacketType() == p2.GetPacketType());
    assert(p.GetData() == p2.GetData());
    assert(p.GetDataLength() == p2.GetDataLength());

    cout << "All tests passed!" << endl;
}