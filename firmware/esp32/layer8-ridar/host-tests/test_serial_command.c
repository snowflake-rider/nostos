#include "check.h"

#include <string.h>

#include "serial_command.h"

static serial_command_t parse_one(const char *input, size_t *emit_count)
{
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

int main(void)
{
    size_t emits = 0U;
    static const struct {
        const char *text;
        serial_command_t command;
    } cases[] = {
        {"status\n", SERIAL_COMMAND_STATUS},
        {"help\n", SERIAL_COMMAND_HELP},
        {"lidar-status\n", SERIAL_COMMAND_LIDAR_STATUS},
        {"lidar-info\r\n", SERIAL_COMMAND_LIDAR_INFO},
        {"lidar-health\n", SERIAL_COMMAND_LIDAR_HEALTH},
        {"lidar-rx-test\n", SERIAL_COMMAND_LIDAR_RX_TEST},
        {"on\n", SERIAL_COMMAND_ON},
        {"factory-reset\n", SERIAL_COMMAND_FACTORY_RESET},
        {"LIDAR-INFO\n", SERIAL_COMMAND_UNKNOWN},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        CHECK(parse_one(cases[i].text, &emits) == cases[i].command);
        CHECK(emits == 1U);
    }
    CHECK(strcmp(serial_command_name(SERIAL_COMMAND_LIDAR_HEALTH),
                 "lidar-health") == 0);
    CHECK(strcmp(serial_command_name(SERIAL_COMMAND_LIDAR_RX_TEST),
                 "lidar-rx-test") == 0);

    serial_command_parser_t parser;
    serial_command_t command = SERIAL_COMMAND_NONE;
    serial_command_parser_init(&parser);
    for (size_t i = 0; i < SERIAL_COMMAND_BUFFER_CAPACITY + 5U; i++) {
        CHECK(!serial_command_parser_feed(&parser, (uint8_t)'x', &command));
    }
    CHECK(serial_command_parser_feed(&parser, (uint8_t)'\n', &command));
    CHECK(command == SERIAL_COMMAND_OVERFLOW);
    return 0;
}
