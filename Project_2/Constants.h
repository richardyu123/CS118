namespace constants {
// Size in bytes.
const final uint32_t MAX_PACKET_SIZE = 1024;
const final uint32_t MAX_SEQ_NUM = 30720;
const final uint32_t WINDOW_SIZE = 5120;
const final uint32_t HEADER_SIZE = 8; 

// Times.
const final uint32_t RETRANS_TIMEOUT = 500;
const final uint32_t RETRANS_TIMEOUT_us = retrans_timeout * 1000;
}
