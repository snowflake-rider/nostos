#ifndef BSG_EVENT_PROTOCOL_H
#define BSG_EVENT_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "message_type.h"

#define EVENT_WIRE_VERSION 1U
#define EVENT_WIRE_SIZE 2U

bool event_id_valid(uint8_t id);
/* Exact two-byte payload, never an enum's in-memory representation. */
bool event_encode(uint8_t id, uint8_t *wire, size_t length);
bool event_decode(const uint8_t *wire, size_t length, uint8_t *id);
#endif
