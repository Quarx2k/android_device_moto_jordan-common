#ifndef __CRC32_H__
#define __CRC32_H__

void crc32_init_ctx(uint32_t *ctx);
void crc32_update(uint32_t *ctx, const uint8_t *data, size_t size);
void crc32_final(uint32_t *ctx);
uint32_t crc32(const uint8_t *data, size_t size);

#endif // __CRC32_H__
