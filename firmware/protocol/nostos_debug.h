#ifndef NOSTOS_DEBUG_H
#define NOSTOS_DEBUG_H

#include "nostos_protocol.h"

#define NOSTOS_HEXDUMP_ROW_BYTES 16U
#define NOSTOS_HEXDUMP_LINE_CAPACITY 72U

/* Debug-only, opt-in output. The protocol never calls this automatically.
 * The callback receives one NUL-terminated line and may stop the dump. */
typedef bool (*nostos_debug_line_fn)(void *context, const char *line);

nostos_result_t nostos_debug_hexdump(const uint8_t *wire, size_t length,
    nostos_debug_line_fn write_line, void *context);

#endif
