#ifndef LIDAR_C1_PROTOCOL_H
#define LIDAR_C1_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LIDAR_C1_REQUEST_SIZE 2U
#define LIDAR_C1_MAX_PAYLOAD 64U
#define LIDAR_C1_RESPONSE_INFO 0x04U
#define LIDAR_C1_RESPONSE_HEALTH 0x06U

typedef enum {
    LIDAR_C1_PARSE_NONE = 0,
    LIDAR_C1_PARSE_RESPONSE,
    LIDAR_C1_PARSE_ERROR,
} lidar_c1_parse_event_t;

typedef struct {
    uint8_t type;
    uint8_t send_mode;
    uint32_t payload_size;
    uint8_t payload[LIDAR_C1_MAX_PAYLOAD];
} lidar_c1_response_t;

typedef struct {
    uint8_t state;
    uint8_t descriptor[5];
    size_t descriptor_length;
    size_t payload_length;
    lidar_c1_response_t response;
} lidar_c1_parser_t;

typedef struct {
    uint8_t model;
    uint8_t firmware_minor;
    uint8_t firmware_major;
    uint8_t hardware;
    uint8_t serial_number[16];
} lidar_c1_info_t;

typedef struct {
    uint8_t status;
    uint16_t error_code;
} lidar_c1_health_t;

void lidar_c1_parser_init(lidar_c1_parser_t *parser);
lidar_c1_parse_event_t lidar_c1_parser_feed(lidar_c1_parser_t *parser,
                                            uint8_t byte,
                                            lidar_c1_response_t *response);
bool lidar_c1_decode_info(const lidar_c1_response_t *response,
                          lidar_c1_info_t *info);
bool lidar_c1_decode_health(const lidar_c1_response_t *response,
                            lidar_c1_health_t *health);
void lidar_c1_make_info_request(uint8_t request[LIDAR_C1_REQUEST_SIZE]);
void lidar_c1_make_health_request(uint8_t request[LIDAR_C1_REQUEST_SIZE]);

#endif
