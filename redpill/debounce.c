/* 김현수 제출 코드 기반. 원문: originals/day24.md */
#include "debounce.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>



// LEN: macro for getting length of arr
#define LEN(arr) (sizeof(arr) / sizeof(((arr)[0])))
// Sample Input
static const bool sample_inputs[] = {
    0, 0, 1, 0, 1, 1, 1, 1, 1, 0};

// Sample Threshold
static const unsigned int sample_threshold = 3;
// debounce: 디바운싱 알고리즘 구현
bool rp24_debounce(rp24_Debouncer *debouncer, bool raw_input);
// display_process: 테이블(스텝, 인풋, 카운터, 아웃풋)
// - 스테이트 변화 감지 메세지 프린트
static void display_process(rp24_Debouncer *debouncer, const bool inputs[], const unsigned int len);
// init_debouncer: 디바운서 초기화
bool rp24_init_debouncer(rp24_Debouncer *debouncer, const unsigned int threshold);

int rp24_demo(void)
{
    rp24_Debouncer dbc;
    if (!rp24_init_debouncer(&dbc, sample_threshold))
    {
        fprintf(stderr, "Failed to initialize Debouncer.\n");
        return EXIT_FAILURE;
    }
    display_process(&dbc, sample_inputs, LEN(sample_inputs));

    return EXIT_SUCCESS;
}

bool rp24_init_debouncer(rp24_Debouncer *debouncer, const unsigned int threshold)
{
    if (debouncer == NULL || threshold == 0)
    {
        return false;
    }

    *debouncer = (rp24_Debouncer){
        .stable_output = false,
        .counter = 0,
        .threshold = threshold,
    };

    return true;
}

bool rp24_debounce(rp24_Debouncer *debouncer, bool raw_input)
{
    // input == output
    if (raw_input == debouncer->stable_output)
    {
        debouncer->counter = 0;
        return false;
    }
    // input != output
    debouncer->counter++;
    if (debouncer->counter >= debouncer->threshold)
    {
        debouncer->stable_output = raw_input;
        debouncer->counter = 0;
        return true;
    }
    return false;
}

static void display_process(rp24_Debouncer *debouncer, const bool inputs[], const unsigned int len)
{
    if (debouncer == NULL || inputs == NULL || len == 0 || debouncer->threshold == 0)
    {
        return;
    }

    printf("=== Day 24: Button Debouncing Logic ===\n");
    printf("Condition: %u consecutive samples required.\n\n",
           debouncer->threshold);
    printf("Step | Raw Input | Counter | Output (Stable)\n");
    printf("-----+-----------+---------+----------------\n");

    for (unsigned int i = 0; i < len; i++)
    {
        const bool raw_input = inputs[i];
        const bool state_changed = rp24_debounce(debouncer, raw_input);

        if (state_changed)
        {
            printf(">> [State Changed] to %u\n",
                   (unsigned int)debouncer->stable_output);
        }

        printf("%4u |     %u     | %7u | %7u\n",
               i + 1,
               (unsigned int)raw_input,
               debouncer->counter,
               (unsigned int)debouncer->stable_output);
    }
}
