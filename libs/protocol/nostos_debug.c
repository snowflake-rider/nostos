#include "nostos_debug.h"

static char hex_digit(uint8_t value)
{
    value=(uint8_t)(value&0x0fU);
    return value<10U?(char)('0'+value):(char)('a'+(value-10U));
}

nostos_result_t nostos_debug_hexdump(const uint8_t *wire, size_t length,
    nostos_debug_line_fn write_line, void *context)
{
    if (!wire || !write_line) return NOSTOS_BAD_ARGUMENT;
    if (!length) return NOSTOS_BAD_LENGTH;
    if (length>NOSTOS_WIRE_MAX) return NOSTOS_TOO_LARGE;

    for (size_t offset=0; offset<length; offset+=NOSTOS_HEXDUMP_ROW_BYTES) {
        char line[NOSTOS_HEXDUMP_LINE_CAPACITY];
        size_t position=0;
        size_t row=length-offset;
        if (row>NOSTOS_HEXDUMP_ROW_BYTES) row=NOSTOS_HEXDUMP_ROW_BYTES;

        for (unsigned shift=12U;; shift-=4U) {
            line[position++]=hex_digit((uint8_t)(offset>>shift));
            if (!shift) break;
        }
        line[position++]=' ';
        line[position++]=' ';
        for (size_t column=0; column<NOSTOS_HEXDUMP_ROW_BYTES; ++column) {
            if (column<row) {
                uint8_t byte=wire[offset+column];
                line[position++]=hex_digit((uint8_t)(byte>>4));
                line[position++]=hex_digit(byte);
                line[position++]=' ';
            } else {
                line[position++]=' ';
                line[position++]=' ';
                line[position++]=' ';
            }
        }
        line[position++]=' ';
        for (size_t column=0; column<row; ++column) {
            uint8_t byte=wire[offset+column];
            line[position++]=(byte>=0x20U && byte<=0x7eU)?(char)byte:'.';
        }
        line[position]='\0';
        if (!write_line(context,line)) return NOSTOS_IO_ERROR;
    }
    return NOSTOS_OK;
}
