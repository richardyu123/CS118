#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdio.h>
#include <string>
#include <stdint.h>

namespace util {
void exit_on_error(const std::string& msg); 
}

namespace constants {
// Size in bytes.
const static uint32_t MAX_PACKET_LEN = 16;
const static uint32_t MAX_SEQ_NUM = 30720;
const static uint32_t WINDOW_SIZE = 80;
const static uint32_t HEADER_SIZE = 8; 

// Times.
const static uint32_t RETRANS_TIMEOUT = 500;
const static uint32_t RETRANS_TIMEOUT_us = RETRANS_TIMEOUT * 1000;
}

#endif
