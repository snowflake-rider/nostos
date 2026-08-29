#include "serial_command.h"

#include <string.h>

static serial_command_t parse_line(const char *line)
{
    static const struct {
        const char *text;
        serial_command_t command;
    } commands[] = {
        {"on", SERIAL_COMMAND_ON},
        {"off", SERIAL_COMMAND_OFF},
        {"on-unack", SERIAL_COMMAND_ON_UNACK},
        {"off-unack", SERIAL_COMMAND_OFF_UNACK},
        {"tx-low", SERIAL_COMMAND_TX_LOW},
        {"tx-normal", SERIAL_COMMAND_TX_NORMAL},
        {"status", SERIAL_COMMAND_STATUS},
        {"help", SERIAL_COMMAND_HELP},
        {"lidar-status", SERIAL_COMMAND_LIDAR_STATUS},
        {"lidar-info", SERIAL_COMMAND_LIDAR_INFO},
        {"lidar-health", SERIAL_COMMAND_LIDAR_HEALTH},
        {"lidar-rx-test", SERIAL_COMMAND_LIDAR_RX_TEST},
        {"factory-reset", SERIAL_COMMAND_FACTORY_RESET},
    };

    if (line[0] == '\0') return SERIAL_COMMAND_EMPTY;
    for (size_t index = 0; index < sizeof(commands) / sizeof(commands[0]); index++) {
        if (strcmp(line, commands[index].text) == 0) return commands[index].command;
    }
    return SERIAL_COMMAND_UNKNOWN;
}

void serial_command_parser_init(serial_command_parser_t *parser)
{
    if (parser != NULL) memset(parser, 0, sizeof(*parser));
}

bool serial_command_parser_feed(serial_command_parser_t *parser, uint8_t byte,
                                serial_command_t *command)
{
    if (parser == NULL || command == NULL) return false;
    if (parser->ignore_next_lf && byte == '\n') {
        parser->ignore_next_lf = false;
        return false;
    }
    parser->ignore_next_lf = false;
    *command = SERIAL_COMMAND_NONE;

    if (byte != '\n' && byte != '\r') {
        if (parser->discarding_overflow) return false;
        if (parser->length < SERIAL_COMMAND_BUFFER_CAPACITY - 1U) {
            parser->buffer[parser->length++] = (char)byte;
        } else {
            parser->discarding_overflow = true;
        }
        return false;
    }

    if (byte == '\r') parser->ignore_next_lf = true;
    if (parser->discarding_overflow) {
        *command = SERIAL_COMMAND_OVERFLOW;
    } else {
        parser->buffer[parser->length] = '\0';
        *command = parse_line(parser->buffer);
    }
    parser->discarding_overflow = false;
    parser->length = 0U;
    parser->buffer[0] = '\0';
    return true;
}

const char *serial_command_name(serial_command_t command)
{
    switch (command) {
    case SERIAL_COMMAND_ON: return "on";
    case SERIAL_COMMAND_OFF: return "off";
    case SERIAL_COMMAND_ON_UNACK: return "on-unack";
    case SERIAL_COMMAND_OFF_UNACK: return "off-unack";
    case SERIAL_COMMAND_TX_LOW: return "tx-low";
    case SERIAL_COMMAND_TX_NORMAL: return "tx-normal";
    case SERIAL_COMMAND_STATUS: return "status";
    case SERIAL_COMMAND_HELP: return "help";
    case SERIAL_COMMAND_LIDAR_STATUS: return "lidar-status";
    case SERIAL_COMMAND_LIDAR_INFO: return "lidar-info";
    case SERIAL_COMMAND_LIDAR_HEALTH: return "lidar-health";
    case SERIAL_COMMAND_LIDAR_RX_TEST: return "lidar-rx-test";
    case SERIAL_COMMAND_FACTORY_RESET: return "factory-reset";
    case SERIAL_COMMAND_EMPTY: return "empty";
    case SERIAL_COMMAND_OVERFLOW: return "overflow";
    case SERIAL_COMMAND_NONE: return "none";
    case SERIAL_COMMAND_UNKNOWN:
    default: return "unknown";
    }
}
