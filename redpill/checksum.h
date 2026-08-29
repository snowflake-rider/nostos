/* 김현수, Redpill Day 7. See README.md for contracts and adaptations. */
#ifndef REDPILL_CHECKSUM_H
#define REDPILL_CHECKSUM_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

uint8_t rp07_xor_checksum(const uint8_t *data, size_t data_size);

/* packet must provide data_size + 1 writable bytes. */
void rp07_update_packet_checksum(uint8_t *packet, size_t data_size);

int rp07_demo(void);

#endif
