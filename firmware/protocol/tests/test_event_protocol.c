#include "event_protocol.h"
#include "check.h"

int main(void)
{
    uint8_t wire[2] = {0};
    uint8_t decoded = 0;
    CHECK(event_encode(MSG_SPEED_DOWN_REQUEST, wire, sizeof(wire)));
    CHECK(wire[0] == 0x01 && wire[1] == 0x10);
    CHECK(event_decode(wire, sizeof(wire), &decoded));
    CHECK(decoded == 0x10);
    const uint8_t ids[] = {0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x30, 0x31};
    for (size_t i = 0; i < sizeof(ids); ++i) {
        CHECK(event_encode(ids[i], wire, 2));
        CHECK(wire[0] == 1 && wire[1] == ids[i]);
        CHECK(event_decode(wire, 2, &decoded) && decoded == ids[i]);
    }
    for (unsigned i = 0; i <= 255; ++i) {
        bool expected = (i >= 0x10 && i <= 0x13) || i == 0x20 || i == 0x21 ||
                        i == 0x30 || i == 0x31;
        CHECK(event_id_valid((uint8_t)i) == expected);
        wire[0] = 1;
        wire[1] = (uint8_t)i;
        decoded = 0xEE;
        CHECK(event_decode(wire, 2, &decoded) == expected);
        if (!expected) CHECK(decoded == 0xEE);
    }
    uint8_t malformed[] = {2, 0x10, 0};
    CHECK(!event_decode(malformed, 2, &decoded));
    malformed[0] = 1;
    CHECK(!event_decode(malformed, 0, &decoded));
    CHECK(!event_decode(malformed, 1, &decoded));
    CHECK(!event_decode(malformed, 3, &decoded));
    CHECK(!event_decode(NULL, 2, &decoded));
    CHECK(!event_decode(malformed, 2, NULL));
    CHECK(!event_encode(0, wire, 2));
    CHECK(!event_encode(0xFF, wire, 2));
    CHECK(!event_encode(0x10, NULL, 2));
    CHECK(!event_encode(0x10, wire, 1));
    CHECK(!event_encode(0x10, wire, 3));
    puts("PASS codec: 8 IDs, all 256 byte values, strict length/version/null checks");
    return 0;
}
