namespace constants {
// Size in bytes.
const uint32_t max_packet_len = 1024;
const uint32_t max_seq_num = 30720;
const uint32_t window_size = 5120;

// Times.
const uint32_t retrans_timeout = 500;
const uint32_t retrans_timeout_us = retrans_timeout * 1000;
}
