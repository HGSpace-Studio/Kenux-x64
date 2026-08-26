#ifndef KERNEL_CHECKSUM_H
#define KERNEL_CHECKSUM_H

#include <arch/types.h>

static inline uint16_t htons(uint16_t val) {
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

uint16_t ip_checksum(const void* data, uint32_t len);
uint32_t tcp_checksum(const tcp_pseudo_header_t* pseudo, const tcp_header_t* tcp, uint32_t tcp_len);
uint32_t udp_checksum(const udp_pseudo_header_t* pseudo, const udp_header_t* udp, uint32_t udp_len);

#endif
