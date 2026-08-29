# Day 24 — 김현수 원문

출처: https://app.notion.com/3612ae7008718299a7f1015021070ab7

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct Debouncer
{
    bool stable_output;
    unsigned int counter;
    unsigned int threshold;
} Debouncer;

// LEN: macro for getting length of arr
#define LEN(arr) (sizeof(arr) / sizeof(((arr)[0])))
// Sample Input
static const bool sample_inputs[] = {
    0, 0, 1, 0, 1, 1, 1, 1, 1, 0};

// Sample Threshold
static const unsigned int sample_threshold = 3;
// debounce: 디바운싱 알고리즘 구현
static bool debounce(Debouncer *debouncer, bool raw_input);
// display_process: 테이블(스텝, 인풋, 카운터, 아웃풋)
// - 스테이트 변화 감지 메세지 프린트
static void display_process(Debouncer *debouncer, const bool inputs[], const unsigned int len);
// init_debouncer: 디바운서 초기화
static bool init_debouncer(Debouncer *debouncer, const unsigned int threshold);

int main(void)
{
    Debouncer dbc;
    if (!init_debouncer(&dbc, sample_threshold))
    {
        fprintf(stderr, "Failed to initialize Debouncer.\n");
        return EXIT_FAILURE;
    }
    display_process(&dbc, sample_inputs, LEN(sample_inputs));

    return EXIT_SUCCESS;
}

static bool init_debouncer(Debouncer *debouncer, const unsigned int threshold)
{
    if (debouncer == NULL || threshold == 0)
    {
        return false;
    }

    *debouncer = (Debouncer){
        .stable_output = false,
        .counter = 0,
        .threshold = threshold,
    };

    return true;
}

static bool debounce(Debouncer *debouncer, bool raw_input)
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

static void display_process(Debouncer *debouncer, const bool inputs[], const unsigned int len)
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
        const bool state_changed = debounce(debouncer, raw_input);

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


````
