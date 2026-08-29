/* 김현수, Redpill Day 26. See README.md for contracts and adaptations. */
#ifndef REDPILL_PRODUCER_CONSUMER_H
#define REDPILL_PRODUCER_CONSUMER_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

#define rp26_BUFFER_SIZE 5

typedef struct {
  unsigned int buffer[rp26_BUFFER_SIZE];
  unsigned int head;      // 다음에 넣을 자리
  unsigned int tail;      // 다음에 뺄 자리
  unsigned int count;     // 지금 들어있는 iterm 갯수
  unsigned int next_item; // 다음에 만들 iterm 번호

  volatile bool producer_waiting;
  volatile bool consumer_waiting;
} rp26_Manager;

bool rp26_produce(rp26_Manager *manager);

bool rp26_consume(rp26_Manager *manager);

int rp26_demo(void);

#endif
