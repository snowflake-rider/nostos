#include "calibration_completed_audio.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); \
    exit(EXIT_FAILURE); } } while (0)

int main(void)
{
    CHECK(calibration_completed_audio_size == 15885U);
    CHECK(calibration_completed_audio_data[0] == 0x49U);
    CHECK(calibration_completed_audio_data[1] == 0x44U);
    CHECK(calibration_completed_audio_data[2] == 0x33U);
    puts("PASS calibration-completed MP3 asset: 15885 bytes, ID3 header");
    return 0;
}
