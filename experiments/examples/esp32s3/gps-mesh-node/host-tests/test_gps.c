#include "gps_codec.h"
#include "gps_receiver.h"
#include "serial_command.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t golden[24] = {
    1,1,0x32,0,4,3,2,1,1,0,0,0,0,0xf1,0x53,0x65,
    0x68,0x31,0x64,0x16,0x20,0x4e,0xaf,0x4b
};

int main(void) {
    gps_packet_t packet;
    assert(gps_decode(golden, sizeof(golden), &packet) == GPS_OK);
    assert(packet.session_id == 0x01020304 && packet.sequence == 1);
    assert(packet.latitude_e7 == 375665000 && packet.longitude_e7 == 1269780000);
    assert(packet.accuracy_dm == 50 && packet.flags == 1);
    uint8_t output[24];
    assert(gps_encode(&packet, output, sizeof(output)) == GPS_OK);
    assert(memcmp(output, golden, sizeof(golden)) == 0);
    gps_receiver_t receiver = {0};
    assert(gps_receiver_accept(&receiver, 5, golden, 24, 1) == GPS_RX_SOURCE_UNSET);
    gps_receiver_set_source(&receiver, 5);
    assert(gps_receiver_accept(&receiver, 6, golden, 24, 1) == GPS_RX_OTHER_SOURCE);
    assert(gps_receiver_accept(&receiver, 5, golden, 24, 1) == GPS_RX_ACCEPTED);
    assert(gps_receiver_accept(&receiver, 5, golden, 24, 9000) == GPS_RX_OLD);
    assert(!gps_receiver_poll_stale(&receiver, 10000));
    assert(gps_receiver_poll_stale(&receiver, 10001));
    assert(!gps_receiver_poll_stale(&receiver, 10002));
    packet.session_id = 9;
    assert(gps_encode(&packet, output, 24) == GPS_OK);
    assert(gps_receiver_accept(&receiver, 5, output, 24, 11000) == GPS_RX_ACCEPTED);
    assert(gps_receiver_accept(&receiver, 5, golden, 24, 12000) == GPS_RX_OLD);
    uint16_t source = 99;
    assert(gps_parse_source_command("gps-source 0x0005", &source) && source == 5);
    assert(!gps_parse_source_command("gps-source 0x0000", &source));
    assert(!gps_parse_source_command("gps-source 0x8000", &source));
    assert(!gps_parse_source_command("gps-source 0x0005junk", &source));
    assert(!gps_parse_source_command("gps-source 5", &source));
    assert(gps_parse_source_command("gps-source 0x7FFF", &source) && source == 0x7fff);
    assert(!gps_parse_source_command(NULL, &source));
    assert(!gps_parse_source_command("gps-source 0x0005", NULL));
    gps_receiver_set_source(&receiver, 6);
    assert(!receiver.has_sample && !receiver.previous_session && receiver.source == 6);

    /* Malformed inputs must not modify the caller's output object. */
    gps_packet_t before = packet;
    for (size_t size = 0; size < 40; ++size) {
        if (size != 24) assert(gps_decode(golden, size, &packet) == GPS_INVALID_LENGTH);
    }
    assert(memcmp(&before, &packet, sizeof(packet)) == 0);
    uint8_t bad[24];
    const unsigned offsets[] = {0,1,3,4,8,12,19,23};
    for (size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); ++i) {
        memcpy(bad, golden, 24);
        unsigned offset = offsets[i];
        if (offset == 4 || offset == 8 || offset == 12) memset(bad + offset, 0, 4);
        else bad[offset] = 0x7f;
        assert(gps_decode(bad, 24, &packet) == GPS_INVALID_FIELDS);
    }
    packet.latitude_e7 = -900000000; packet.longitude_e7 = -1800000000;
    packet.accuracy_dm = 500; packet.sequence = UINT32_MAX;
    assert(gps_encode(&packet, output, 24) == GPS_OK);
    gps_packet_t decoded;
    uint8_t unaligned[25]; memcpy(unaligned + 1, output, 24);
    assert(gps_decode(unaligned + 1, 24, &decoded) == GPS_OK);
    assert(decoded.latitude_e7 == -900000000 && decoded.longitude_e7 == -1800000000);
    assert(gps_decode(NULL, 24, &decoded) == GPS_INVALID_FIELDS);
    assert(gps_decode(golden, 24, NULL) == GPS_INVALID_FIELDS);
    assert(gps_encode(NULL, output, 24) == GPS_INVALID_FIELDS);
    assert(gps_encode(&packet, NULL, 24) == GPS_INVALID_FIELDS);

    serial_command_parser_t parser;
    serial_command_parser_init(&parser);
    serial_command_t command = SERIAL_COMMAND_NONE;
    const char *line = "gps-source 0x1234\r\n";
    unsigned emitted = 0;
    for (size_t i = 0; line[i]; ++i) {
        if (serial_command_parser_feed(&parser, (uint8_t)line[i], &command)) {
            ++emitted; assert(command == SERIAL_COMMAND_GPS_SOURCE && parser.gps_source == 0x1234);
        }
    }
    assert(emitted == 1);
    for (unsigned i = 0; i < 80; ++i) assert(!serial_command_parser_feed(&parser, 'a', &command));
    assert(serial_command_parser_feed(&parser, '\n', &command) && command == SERIAL_COMMAND_OVERFLOW);
    const char malicious[] = "gps-source 0x1234\0junk\n";
    for (size_t i = 0; i < sizeof(malicious)-1; ++i) {
        if (serial_command_parser_feed(&parser, (uint8_t)malicious[i], &command))
            assert(command != SERIAL_COMMAND_GPS_SOURCE);
    }
    const char *reset = "factory-reset\n";
    for (size_t i = 0; reset[i]; ++i) {
        if (serial_command_parser_feed(&parser, (uint8_t)reset[i], &command)) assert(command == SERIAL_COMMAND_UNKNOWN);
    }
    puts("GPS_C_TESTS=PASS");
}
