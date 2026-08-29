#include "lidar_c1_protocol.h"

#include <string.h>

enum {
    PARSER_SYNC_A5 = 0,
    PARSER_SYNC_5A,
    PARSER_DESCRIPTOR,
    PARSER_PAYLOAD,
};

static void reset_parser(lidar_c1_parser_t *parser)
{
    parser->state = PARSER_SYNC_A5;
    parser->descriptor_length = 0U;
    parser->payload_length = 0U;
    memset(&parser->response, 0, sizeof(parser->response));
}

void lidar_c1_parser_init(lidar_c1_parser_t *parser)
{
    if (parser != NULL) reset_parser(parser);
}

lidar_c1_parse_event_t lidar_c1_parser_feed(lidar_c1_parser_t *parser,
                                            uint8_t byte,
                                            lidar_c1_response_t *response)
{
    if (parser == NULL || response == NULL) return LIDAR_C1_PARSE_ERROR;

    switch (parser->state) {
    case PARSER_SYNC_A5:
        if (byte == 0xA5U) parser->state = PARSER_SYNC_5A;
        return LIDAR_C1_PARSE_NONE;

    case PARSER_SYNC_5A:
        if (byte == 0x5AU) {
            parser->state = PARSER_DESCRIPTOR;
            parser->descriptor_length = 0U;
        } else if (byte != 0xA5U) {
            parser->state = PARSER_SYNC_A5;
        }
        return LIDAR_C1_PARSE_NONE;

    case PARSER_DESCRIPTOR:
        parser->descriptor[parser->descriptor_length++] = byte;
        if (parser->descriptor_length < sizeof(parser->descriptor)) {
            return LIDAR_C1_PARSE_NONE;
        }
        {
            const uint32_t size_and_mode =
                (uint32_t)parser->descriptor[0] |
                ((uint32_t)parser->descriptor[1] << 8U) |
                ((uint32_t)parser->descriptor[2] << 16U) |
                ((uint32_t)parser->descriptor[3] << 24U);
            parser->response.payload_size = size_and_mode & 0x3FFFFFFFU;
            parser->response.send_mode = (uint8_t)(size_and_mode >> 30U);
            parser->response.type = parser->descriptor[4];
        }
        if (parser->response.payload_size > LIDAR_C1_MAX_PAYLOAD) {
            reset_parser(parser);
            return LIDAR_C1_PARSE_ERROR;
        }
        if (parser->response.payload_size == 0U) {
            *response = parser->response;
            reset_parser(parser);
            return LIDAR_C1_PARSE_RESPONSE;
        }
        parser->payload_length = 0U;
        parser->state = PARSER_PAYLOAD;
        return LIDAR_C1_PARSE_NONE;

    case PARSER_PAYLOAD:
        parser->response.payload[parser->payload_length++] = byte;
        if (parser->payload_length < parser->response.payload_size) {
            return LIDAR_C1_PARSE_NONE;
        }
        *response = parser->response;
        reset_parser(parser);
        return LIDAR_C1_PARSE_RESPONSE;

    default:
        reset_parser(parser);
        return LIDAR_C1_PARSE_ERROR;
    }
}

bool lidar_c1_decode_info(const lidar_c1_response_t *response,
                          lidar_c1_info_t *info)
{
    if (response == NULL || info == NULL ||
        response->type != LIDAR_C1_RESPONSE_INFO ||
        response->send_mode != 0U || response->payload_size != 20U) {
        return false;
    }
    info->model = response->payload[0];
    info->firmware_minor = response->payload[1];
    info->firmware_major = response->payload[2];
    info->hardware = response->payload[3];
    memcpy(info->serial_number, &response->payload[4], sizeof(info->serial_number));
    return true;
}

bool lidar_c1_decode_health(const lidar_c1_response_t *response,
                            lidar_c1_health_t *health)
{
    if (response == NULL || health == NULL ||
        response->type != LIDAR_C1_RESPONSE_HEALTH ||
        response->send_mode != 0U || response->payload_size != 3U) {
        return false;
    }
    health->status = response->payload[0];
    health->error_code = (uint16_t)response->payload[1] |
                         ((uint16_t)response->payload[2] << 8U);
    return true;
}

void lidar_c1_make_info_request(uint8_t request[LIDAR_C1_REQUEST_SIZE])
{
    if (request == NULL) return;
    request[0] = 0xA5U;
    request[1] = 0x50U;
}

void lidar_c1_make_health_request(uint8_t request[LIDAR_C1_REQUEST_SIZE])
{
    if (request == NULL) return;
    request[0] = 0xA5U;
    request[1] = 0x52U;
}
