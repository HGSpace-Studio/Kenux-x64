#ifndef KAPI_CRC_H
#define KAPI_CRC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t kapi_crc16(const void* data, size_t len);

uint32_t kapi_crc32(const void* data, size_t len);

uint16_t kapi_crc16_ccitt(const void* data, size_t len);

uint32_t kapi_crc32c(const void* data, size_t len);

uint16_t kapi_crc16_update(uint16_t crc, const void* data, size_t len);

uint32_t kapi_crc32_update(uint32_t crc, const void* data, size_t len);

int kapi_crc16_check(const void* data, size_t len, uint16_t expected);

int kapi_crc32_check(const void* data, size_t len, uint32_t expected);

#ifdef __cplusplus
}
#endif

#endif