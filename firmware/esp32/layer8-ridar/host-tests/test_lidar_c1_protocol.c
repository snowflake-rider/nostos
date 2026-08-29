#include "check.h"

#include <string.h>

#include "lidar_c1_protocol.h"

static lidar_c1_parse_event_t feed_bytes(lidar_c1_parser_t *parser,
                                         const uint8_t *bytes, size_t length,
                                         lidar_c1_response_t *response)
{
    lidar_c1_parse_event_t event = LIDAR_C1_PARSE_NONE;
    for (size_t i = 0; i < length; i++) {
        const lidar_c1_parse_event_t next =
            lidar_c1_parser_feed(parser, bytes[i], response);
        if (next != LIDAR_C1_PARSE_NONE) event = next;
    }
    return event;
}

int main(void)
{
    uint8_t request[LIDAR_C1_REQUEST_SIZE];
    lidar_c1_make_info_request(request);
    CHECK(request[0] == 0xA5U && request[1] == 0x50U);
    lidar_c1_make_health_request(request);
    CHECK(request[0] == 0xA5U && request[1] == 0x52U);

    static const uint8_t info_packet[] = {
        0x00U, 0xA5U, 0xA5U, 0x5AU, 0x14U, 0x00U, 0x00U, 0x00U, 0x04U,
        0x21U, 0x07U, 0x01U, 0x02U,
        0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU,
    };
    lidar_c1_parser_t parser;
    lidar_c1_response_t response;
    lidar_c1_parser_init(&parser);
    CHECK(feed_bytes(&parser, info_packet, sizeof(info_packet), &response) ==
          LIDAR_C1_PARSE_RESPONSE);
    lidar_c1_info_t info;
    CHECK(lidar_c1_decode_info(&response, &info));
    CHECK(info.model == 0x21U);
    CHECK(info.firmware_major == 1U && info.firmware_minor == 7U);
    CHECK(info.hardware == 2U);
    CHECK(info.serial_number[0] == 0U && info.serial_number[15] == 0x0FU);

    static const uint8_t health_packet[] = {
        0xA5U, 0x5AU, 0x03U, 0x00U, 0x00U, 0x00U, 0x06U,
        0x01U, 0x34U, 0x12U,
    };
    CHECK(feed_bytes(&parser, health_packet, sizeof(health_packet), &response) ==
          LIDAR_C1_PARSE_RESPONSE);
    lidar_c1_health_t health;
    CHECK(lidar_c1_decode_health(&response, &health));
    CHECK(health.status == 1U && health.error_code == 0x1234U);

    static const uint8_t oversized[] = {
        0xA5U, 0x5AU, 0x41U, 0x00U, 0x00U, 0x00U, 0x04U,
    };
    CHECK(feed_bytes(&parser, oversized, sizeof(oversized), &response) ==
          LIDAR_C1_PARSE_ERROR);
    CHECK(feed_bytes(&parser, health_packet, sizeof(health_packet), &response) ==
          LIDAR_C1_PARSE_RESPONSE);
    CHECK(lidar_c1_decode_health(&response, &health));

    response.send_mode = 1U;
    CHECK(!lidar_c1_decode_health(&response, &health));
    return 0;
}
