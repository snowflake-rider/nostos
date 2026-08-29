/* 김현수 제출 코드 기반. 원문: originals/day07.md */
#include "checksum.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/**
 **입력:** 바이트 배열 `[0x01, 0x04, 0x10, 0x20, 0x30, 0x40]`, 길이
- **출력:** XOR 누적 체크섬 값
- **제약조건:** 포인터 연산 사용.
- **실행결과:**

```c
=== Day 7: XOR Checksum Calculation ===

[TX] Sending Packet...
     Data: 0x01 0x04 0x10 0x20 0x30 0x40
     Calculated Checksum: 0x45

[RX] Receiving Normal Packet...
     >> Verification SUCCESS (Result: 0x00)

[RX] Receiving Corrupted Packet (Noise injected)...
     Corrupted Data: 0x01 0x04 0xEF 0x20 0x30 0x40 0x45
     >> Verification FAIL (Result: 0xFF)
     >> Error detected! Discarding packet.
```
 */

// DONE: [🟢] Use Pointer

uint8_t rp07_xor_checksum(const uint8_t *data, size_t data_size);
void rp07_update_packet_checksum(uint8_t *packet, size_t data_size);
static void transmission(uint8_t *packet, size_t data_size);
static void reception(const uint8_t *packet, size_t packet_size);
static void print_byte_array(const uint8_t *data, size_t data_size);

int rp07_demo(void)
{
    enum
    {
        DATA_SIZE = 6,
        PACKET_SIZE = DATA_SIZE + 1
    };

    printf("=== Day 7: XOR Checksum Calculation ===\n\n");

    // CASE: VALID PACKET
    // 1. Start with six data bytes and one reserved checksum byte.
    uint8_t sample_packet[PACKET_SIZE] = {
        0x01, 0x04, 0x10, 0x20, 0x30, 0x40};
    // 2. TX updates the reserved sample_packet[6] byte with the checksum.
    transmission(sample_packet, DATA_SIZE);
    // 3. RX verifies all seven bytes, including the checksum.
    reception(sample_packet, PACKET_SIZE);

    // CASE: CORRUPTED PACKET
    // 1. Simulate noise after the checksum byte has been updated.
    sample_packet[2] = 0xEF;
    // 2. RX verifies the corrupted seven-byte packet.
    reception(sample_packet, PACKET_SIZE);

    return 0;
}

/*
 * data: pointer to the first byte
 * data_size: number of bytes to include
 * return: XOR checksum of those bytes
 */
uint8_t rp07_xor_checksum(const uint8_t *data, size_t data_size)
{
    uint8_t checksum = 0x00;

    for (size_t i = 0; i < data_size; ++i)
    {
        checksum ^= *(data + i);
    }

    return checksum;
}

static void print_byte_array(const uint8_t *data, size_t data_size)
{
    for (size_t i = 0; i < data_size; ++i)
    {
        printf("0x%02X", (unsigned int)*(data + i));

        if (i < data_size - 1U)
        {
            putchar(' ');
        }
    }
}

void rp07_update_packet_checksum(uint8_t *packet, size_t data_size)
{
    // Store the checksum in the reserved packet[data_size] byte.
    const uint8_t checksum = rp07_xor_checksum(packet, data_size);
    *(packet + data_size) = checksum;
}

static void transmission(uint8_t *packet, size_t data_size)
{
    printf("[TX] Sending Packet...\n");
    printf("     Data: ");
    print_byte_array(packet, data_size);
    printf("\n");
    rp07_update_packet_checksum(packet, data_size);
    printf("     Calculated Checksum: 0x%02X\n",
           (unsigned int)packet[data_size]);
}

static void reception(const uint8_t *packet, size_t packet_size)
{
    const uint8_t verification_result = rp07_xor_checksum(packet, packet_size);
    const bool is_corrupted = (verification_result != 0x00U); // A valid packet XORs to zero.

    // CASE: NORMAL PACKET
    if (!is_corrupted)
    {
        printf("\n[RX] Receiving Normal Packet...\n");
        printf("     >> Verification SUCCESS (Result: 0x%02X)\n",
               (unsigned int)verification_result);
    }

    // CASE: CORRUPTED PACKET
    else
    {
        printf("\n[RX] Receiving Corrupted Packet (Noise injected)...\n");
        printf("     Corrupted Data: ");
        print_byte_array(packet, packet_size);
        printf("\n");
        printf("     >> Verification FAIL (Result: 0x%02X)\n",
               (unsigned int)verification_result);
        printf("     >> Error detected! Discarding packet.\n");
    }
}
