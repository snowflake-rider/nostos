#ifndef SERIAL_COMMAND_H
#define SERIAL_COMMAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SERIAL_COMMAND_BUFFER_CAPACITY 32U

typedef enum {
  SERIAL_COMMAND_NONE = 0,
  SERIAL_COMMAND_TX_LOW,
  SERIAL_COMMAND_TX_NORMAL,
  SERIAL_COMMAND_STATUS,
  SERIAL_COMMAND_EMPTY,
  SERIAL_COMMAND_UNKNOWN,
  SERIAL_COMMAND_OVERFLOW,
} serial_command_t;

typedef struct {
  char buffer[SERIAL_COMMAND_BUFFER_CAPACITY];
  size_t length;
  bool discarding_overflow;
  bool ignore_next_lf;
} serial_command_parser_t;

void serial_command_parser_init(serial_command_parser_t *parser);

bool serial_command_parser_feed(serial_command_parser_t *parser, uint8_t byte,
                                serial_command_t *command);

const char *serial_command_name(serial_command_t command);

#endif
