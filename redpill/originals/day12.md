# Day 12 — 김현수 원문

출처: https://app.notion.com/8f12ae70087183d0b8f6014c6f79c766

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stddef.h>
#include <stdio.h>

/**
my_offsetof(type, member)

목적: 구조체 시작 위치부터 멤버까지 떨어진 바이트 수를 구한다.
- type: 구조체 타입
- member: 구조체 멤버

1. (type *)0
   : 0을 type * 널 포인터로 바꾼다.
     계산할 때 구조체의 시작 주소를 0이라고 가정한다.
2. ((type *)0)->member
   : 구조체에서 member의 자리를 선택한다. 값을 읽는 것은 아니다.
3. &(((type *)0)->member)
   : member 자리의 주소를 구한다.
4. (size_t)&(((type *)0)->member)
   : 주소를 size_t로 바꾼다. 시작 주소가 0이므로 이 값이 offset이다.

예: member가 시작점에서 4바이트 떨어져 있다면 offset은 4이다.

주소 0에서 구조체가 시작한다고 가정하고, 멤버의 위치를 선택한 다음 그 위치의 주소값을 구한다.
 */
#define my_offsetof(type, member) ((size_t)&(((type *)0)->member))

// Sample 구조체
typedef struct
{
  char a;
  int b;
  double c;
} Sample;

// Sample 멤버별 오프셋을 저장
typedef struct
{
  size_t offset_a;
  size_t offset_b;
  size_t offset_c;
} SampleOffset;

// 오프셋 계산 출처: 표준 offsetof 또는 my_offsetof
typedef enum
{
  OFFSET_STANDARD,
  OFFSET_MY_MACRO
} OffsetSource;

// print_offsets: 각 멤버의 오프셋 출력
static void print_offsets(OffsetSource source, const SampleOffset *offsets);

// compare_offsets: 표준 offsetof와 my_offsetof의 결과 비교
static void compare_offsets(
    const SampleOffset *expected_offsets,
    const SampleOffset *actual_offsets);

int main(void)
{
  // 표준 <stddef.h>의 offsetof 매크로로 계산한 값
  const SampleOffset standard_offsets = {.offset_a = offsetof(Sample, a),
                                         .offset_b = offsetof(Sample, b),
                                         .offset_c = offsetof(Sample, c)};
  const SampleOffset my_offsets = {.offset_a = my_offsetof(Sample, a),
                                   .offset_b = my_offsetof(Sample, b),
                                   .offset_c = my_offsetof(Sample, c)};

  printf("=== Day 12: offsetof Implementation ===\n\n");

  printf("Struct Size: %zu bytes\n\n", sizeof(Sample));
  print_offsets(OFFSET_STANDARD, &standard_offsets);
  printf("\n-----------------------------\n\n");

  print_offsets(OFFSET_MY_MACRO, &my_offsets);

  printf("\n");

  compare_offsets(&standard_offsets, &my_offsets);

  return 0;
}
static void print_offsets(OffsetSource source, const SampleOffset *offsets)
{
  const char *label =
      (source == OFFSET_STANDARD) ? "Standard" : "My Macro";

  printf("[%s] Offset of a: %zu\n", label, offsets->offset_a);
  printf("[%s] Offset of b: %zu\n", label, offsets->offset_b);
  printf("[%s] Offset of c: %zu\n", label, offsets->offset_c);
}

static void compare_offsets(const SampleOffset *expected_offsets, const SampleOffset *actual_offsets)
{
  if ((expected_offsets->offset_a == actual_offsets->offset_a) &&
      (expected_offsets->offset_b == actual_offsets->offset_b) &&
      (expected_offsets->offset_c == actual_offsets->offset_c))
  {
    printf(">> Success! Implementation is correct.\n");
  }
  else
  {
    printf(">> Fail! Implementation is incorrect.\n");
  }
}


````
