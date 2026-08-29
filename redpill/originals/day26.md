# Day 26 — 김현수 원문

출처: https://app.notion.com/79a2ae7008718351bacc012f2dfbe17e

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { BUFFER_SIZE = 5, SIMULATION_STEPS = 12, PRODUCE_CHANCE_PERCENT = 80 };

typedef struct {
  unsigned int buffer[BUFFER_SIZE];
  unsigned int head;      // 다음에 넣을 자리
  unsigned int tail;      // 다음에 뺄 자리
  unsigned int count;     // 지금 들어있는 iterm 갯수
  unsigned int next_item; // 다음에 만들 iterm 번호

  volatile bool producer_waiting;
  volatile bool consumer_waiting;
} Manager;

// update full/empty status
static void update_waiting_status(Manager *manager);
// check random chance
static bool event_occurs(unsigned int chance_percent);
// put an iterm in buffer
static bool produce(Manager *manager);
// take oldest iterm
static bool consume(Manager *manager);
// run random simulation
static void simulation(Manager *manager, unsigned int simulation_steps);

int main(void) {
  Manager manager = {
      .buffer = {0},
      .head = 0,
      .tail = 0,
      .count = 0,
      .next_item = 1,
      .producer_waiting = false,
      .consumer_waiting = true,
  };

  // random seed
  srand((unsigned int)time(NULL));
  simulation(&manager, SIMULATION_STEPS);

  return 0;
}

static void update_waiting_status(Manager *manager) {
  manager->producer_waiting = (manager->count == BUFFER_SIZE);
  manager->consumer_waiting = (manager->count == 0);
}

static bool event_occurs(unsigned int chance_percent) {
  return (unsigned int)(rand() % 100) < chance_percent;
}

static bool produce(Manager *manager) {
  // Update Status
  update_waiting_status(manager);

  if (manager->producer_waiting) {
    printf("  >> [Buffer Full!] Producer waits... (Count: %u)\n",
           manager->count);
    return false;
  }

  unsigned int item = manager->next_item;
  manager->buffer[manager->head] = item;
  manager->head = (manager->head + 1) % BUFFER_SIZE;
  ++(manager->count);
  ++(manager->next_item);

  // Update Status
  update_waiting_status(manager);

  printf("[PROD] Produced Item %u (Head: %u, Count: %u)\n", item, manager->head,
         manager->count);
  return true;
}

static bool consume(Manager *manager) {
  // Update Status
  update_waiting_status(manager);

  if (manager->consumer_waiting) {
    printf("  >> [Buffer Empty!] Consumer waits... (Count: %u)\n",
           manager->count);
    return false;
  }

  unsigned int item = manager->buffer[manager->tail];
  manager->tail = (manager->tail + 1) % BUFFER_SIZE;
  --(manager->count);
  // Update Status
  update_waiting_status(manager);

  printf("[CONS] Consumed Item %u (Tail: %u, Count: %u)\n", item, manager->tail,
         manager->count);
  return true;
}

static void simulation(Manager *manager, unsigned int simulation_steps) {
  printf("=== Day 26: Producer-Consumer Simulation ===\n");
  printf("Buffer Size: %d\n\n", BUFFER_SIZE);

  for (unsigned int step = 1; step <= simulation_steps; ++step) {
    bool producer_ready = event_occurs(PRODUCE_CHANCE_PERCENT);

    if (producer_ready) {
      produce(manager);
    } else {
      consume(manager);
    }
  }
}


````
