#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "serial_command.h"

static serial_command_t parse_one(const char *input, size_t *emit_count) {
  serial_command_parser_t parser;
  serial_command_t command = SERIAL_COMMAND_NONE;

  serial_command_parser_init(&parser);
  *emit_count = 0U;
  for (size_t index = 0; index < strlen(input); index++) {
    if (serial_command_parser_feed(&parser, (uint8_t)input[index], &command)) {
      (*emit_count)++;
    }
  }
  return command;
}

int main(void) {
  size_t emit_count = 0U;
  static const struct {
    const char *input;
    serial_command_t expected;
  } cases[] = {
      {"on-unack\n", SERIAL_COMMAND_ON_UNACK},
      {"off-unack\n", SERIAL_COMMAND_OFF_UNACK},
      {"tx-low\n", SERIAL_COMMAND_TX_LOW},
      {"tx-normal\n", SERIAL_COMMAND_TX_NORMAL},
      {"status\n", SERIAL_COMMAND_STATUS},
      {"factory-reset\n", SERIAL_COMMAND_FACTORY_RESET},
      {"\n", SERIAL_COMMAND_EMPTY},
      {"ON\n", SERIAL_COMMAND_UNKNOWN},
      {"factory-reset-now\n", SERIAL_COMMAND_UNKNOWN},
  };

  assert(parse_one("on\n", &emit_count) == SERIAL_COMMAND_ON);
  assert(emit_count == 1U);

  assert(parse_one("off\r\n", &emit_count) == SERIAL_COMMAND_OFF);
  assert(emit_count == 1U);

  for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
    assert(parse_one(cases[index].input, &emit_count) ==
           cases[index].expected);
    assert(emit_count == 1U);
  }

  assert(strcmp(serial_command_name(SERIAL_COMMAND_TX_LOW), "tx-low") == 0);
  assert(strcmp(serial_command_name(SERIAL_COMMAND_TX_NORMAL), "tx-normal") ==
         0);

  serial_command_parser_t parser;
  serial_command_t command = SERIAL_COMMAND_NONE;
  serial_command_parser_init(&parser);
  for (size_t index = 0; index < SERIAL_COMMAND_BUFFER_CAPACITY + 5U; index++) {
    assert(!serial_command_parser_feed(&parser, (uint8_t)'x', &command));
  }
  assert(serial_command_parser_feed(&parser, (uint8_t)'\n', &command));
  assert(command == SERIAL_COMMAND_OVERFLOW);

  static const char recovery[] = "status\n";
  for (size_t index = 0; index < sizeof(recovery) - 1U; index++) {
    if (serial_command_parser_feed(&parser, (uint8_t)recovery[index],
                                   &command)) {
      assert(command == SERIAL_COMMAND_STATUS);
    }
  }

  serial_command_parser_init(&parser);
  static const char repeated[] = "on\noff\n";
  size_t repeated_emits = 0U;
  for (size_t index = 0; index < sizeof(repeated) - 1U; index++) {
    if (serial_command_parser_feed(&parser, (uint8_t)repeated[index],
                                   &command)) {
      assert(command ==
             (repeated_emits == 0U ? SERIAL_COMMAND_ON : SERIAL_COMMAND_OFF));
      repeated_emits++;
    }
  }
  assert(repeated_emits == 2U);

  puts("LAYER7_SERIAL_COMMAND_TESTS=PASS");
  return 0;
}
