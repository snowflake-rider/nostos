/* 김현수 제출 코드 기반. 원문: originals/day26.md */
#include "producer_consumer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { SIMULATION_STEPS = 12, PRODUCE_CHANCE_PERCENT = 80 };



// update full/empty status
static void update_waiting_status(rp26_Manager *manager);
// check random chance
static bool event_occurs(unsigned int chance_percent);
// put an iterm in buffer
bool rp26_produce(rp26_Manager *manager);
// take oldest iterm
bool rp26_consume(rp26_Manager *manager);
// run random simulation
static void simulation(rp26_Manager *manager, unsigned int simulation_steps);

int rp26_demo(void) {
  rp26_Manager manager = {
      .buffer = {0},
      .head = 0,
      .tail = 0,
      .count = 0,
      .next_item = 1,
      .producer_waiting = false,
      .consumer_waiting = true,
  };

  // random seed
  srand(1U); /* Repeatable demo; original used time(NULL). */
  simulation(&manager, SIMULATION_STEPS);

  return 0;
}

static void update_waiting_status(rp26_Manager *manager) {
  manager->producer_waiting = (manager->count == rp26_BUFFER_SIZE);
  manager->consumer_waiting = (manager->count == 0);
}

static bool event_occurs(unsigned int chance_percent) {
  return (unsigned int)(rand() % 100) < chance_percent;
}

bool rp26_produce(rp26_Manager *manager) {
  // Update Status
  update_waiting_status(manager);

  if (manager->producer_waiting) {
    printf("  >> [Buffer Full!] Producer waits... (Count: %u)\n",
           manager->count);
    return false;
  }

  unsigned int item = manager->next_item;
  manager->buffer[manager->head] = item;
  manager->head = (manager->head + 1) % rp26_BUFFER_SIZE;
  ++(manager->count);
  ++(manager->next_item);

  // Update Status
  update_waiting_status(manager);

  printf("[PROD] Produced Item %u (Head: %u, Count: %u)\n", item, manager->head,
         manager->count);
  return true;
}

bool rp26_consume(rp26_Manager *manager) {
  // Update Status
  update_waiting_status(manager);

  if (manager->consumer_waiting) {
    printf("  >> [Buffer Empty!] Consumer waits... (Count: %u)\n",
           manager->count);
    return false;
  }

  unsigned int item = manager->buffer[manager->tail];
  manager->tail = (manager->tail + 1) % rp26_BUFFER_SIZE;
  --(manager->count);
  // Update Status
  update_waiting_status(manager);

  printf("[CONS] Consumed Item %u (Tail: %u, Count: %u)\n", item, manager->tail,
         manager->count);
  return true;
}

static void simulation(rp26_Manager *manager, unsigned int simulation_steps) {
  printf("=== Day 26: Producer-Consumer Simulation ===\n");
  printf("Buffer Size: %d\n\n", rp26_BUFFER_SIZE);

  for (unsigned int step = 1; step <= simulation_steps; ++step) {
    bool producer_ready = event_occurs(PRODUCE_CHANCE_PERCENT);

    if (producer_ready) {
      rp26_produce(manager);
    } else {
      rp26_consume(manager);
    }
  }
}
