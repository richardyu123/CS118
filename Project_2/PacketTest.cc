#include <cassert>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

    // Tests header and final data;
    auto full_data = p.GetPacketData();
    auto full_data_size = p.GetPacketLength();
    auto header_p_num = static_cast<uint16_t>((unsigned char)full_data[0] << 8);
    header_p_num |= static_cast<uint16_t>((unsigned char)full_data[1]);
    auto header_p_type = static_cast<Packet::packet_t>(
            (unsigned char)full_data[2]);
    auto header_win_size = static_cast<uint16_t>((unsigned char)full_data[3] << 8);
    header_win_size |= static_cast<uint16_t>((unsigned char)full_data[4]);
    assert(header_p_num == pkt_num);
    assert(header_p_type == Packet::SYN);
    assert(header_win_size == win_size);

    cout << "All tests passed!" << endl;
}
