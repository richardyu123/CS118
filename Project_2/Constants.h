#include <stdint.h>

namespace constants {
// Size in bytes.
const static uint32_t MAX_PACKET_LEN = 1024;
const static uint32_t MAX_SEQ_NUM = 30720;
const static uint32_t WINDOW_SIZE = 5120;
const static uint32_t HEADER_SIZE = 8; 

// Times.
const static uint32_t RETRANS_TIMEOUT = 500;
const static uint32_t RETRANS_TIMEOUT_us = RETRANS_TIMEOUT * 1000;
}
