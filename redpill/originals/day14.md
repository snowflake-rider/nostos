# Day 14 — 김현수 원문

출처: https://app.notion.com/6532ae70087182e0908b81508fcea35c

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdint.h>


// IS_PRINTABLE_ASCII: 매크로를 써서 ASCII 출력가능한 byte 판별
// 출력 가능한 ASCII 범위는 0x20(' ')부터 0x7E('~')까지다.
#define IS_PRINTABLE_ASCII(byte) \
    ((byte) >= 0x20 && (byte) <= 0x7E)

// SensorData 구조체:
// sensor_name: 이름을 포인터가 아닌 구조체 내부 12바이트 배열에 저장한다.
// "Sensor A" 뒤의 남는 칸은 NUL(0x00)로 채워진다.
typedef struct
{
    uint32_t hex_num;
    char sensor_name[12];
    float reading;
} SensorData;

// hexdump: debug 툴과 같이 hexdump
// 임의의 메모리를 수정하지 않고
// offset | hex bytes | ASCII 형식으로 출력한다.
static void hexdump(const char *data_type, const void *data, const size_t size);

int main(void)
{
    // Test Data
    const char my_text[] = "Hello Embedded World! This is Hexdump.";
    const SensorData sensor = {.hex_num = 0x12345678, .sensor_name = "Sensor A", .reading = 3.14f};

    hexdump("String Dump", my_text, sizeof(my_text));
    hexdump("Struct Dump", &sensor, sizeof(sensor));

    return 0;
}

static void hexdump(const char *data_type, const void *data, const size_t size)
{
    // bytes
    // void *는 직접 역참조할 수 없으므로,
    // 메모리를 읽기 전용 1바이트 단위로 해석한다.

    const unsigned char *bytes = data;

    // 덤프 제목 출력
    printf("%s:\n", data_type);
    for (size_t byte_offset = 0; byte_offset < size; byte_offset += 16)
    {
        // Byte Offset
        printf("  %04zx  ", byte_offset);

        // 한 행에는 최대 16바이트만 출력한다.
        size_t remaining = size - byte_offset;             // 현재 행에 실제로 남아 있는 바이트 수를 계산한다.
        size_t row_size = remaining < 16 ? remaining : 16; // 한 행에는 최대 16바이트만 출력한다.

        // Hex Numbers
        for (size_t column = 0; column < 16; column++)
        {
            if (column < row_size)
            {
                printf("%02x ", (unsigned int)bytes[byte_offset + column]);
            }
            else
            {
                printf("   ");
            }
        }
        putchar(' ');
        // ASCII
        for (size_t column = 0; column < row_size; column++)
        {
            unsigned char byte = bytes[byte_offset + column];
            putchar(IS_PRINTABLE_ASCII(byte) ? byte : '.');
        }

        putchar('\n');
    }
    putchar('\n');
}


````
