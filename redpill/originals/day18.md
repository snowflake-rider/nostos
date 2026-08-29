# Day 18 — 김현수 원문

출처: https://app.notion.com/91d2ae700871837bbcff01a95657c426

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
  CAPACITY = 5,
  CAP_MAX = 100,
};

typedef struct Task {
  uint8_t task_id;
  uint8_t priority;
} Task;

// MinHeap (Min on the top, two children have higher priority number)
// Priority Order: A task of Lower Priority Number precedes the task of Higher
// Priority Number e.g. Task(5,0) >>> Task(3,5) >>> Task(2,10)
typedef struct TaskScheduler {
  Task *tasks;
  size_t capacity;
  size_t task_count;
} TaskScheduler;

static void swap(Task *task_x, Task *task_y);
static bool init_task_scheduler(TaskScheduler *ts, const size_t capacity);
static void sift_up(TaskScheduler *ts, size_t child_idx);
static void sift_down(TaskScheduler *ts, size_t parent_idx);
static bool extract(TaskScheduler *ts, Task *task);
static bool insert(TaskScheduler *ts, const Task *new_task);

int main(void) {

  TaskScheduler ts = (TaskScheduler){0};
  // Initialize TaskScheduler

  if (!init_task_scheduler(&ts, CAPACITY)) {
    return EXIT_FAILURE;
  }
  printf("=== Day 18: Task Scheduler (Min Heap) ===\n\n");
  // Insert Tasks to TaskScheduler
  const Task task_list[CAPACITY] = {
      {.task_id = 1, .priority = 50},  {.task_id = 2, .priority = 10},
      {.task_id = 3, .priority = 5},   {.task_id = 5, .priority = 0},
      {.task_id = 4, .priority = 100},
  };
  printf("Tasks pushed: ");
  for (size_t i = 0; i < CAPACITY; ++i) {

    if (!insert(&ts, &task_list[i])) {
      free(ts.tasks);
      return EXIT_FAILURE;
    }
    printf("(%u, %u)", task_list[i].task_id, task_list[i].priority);
    if (i < (CAPACITY - 1)) {
      printf(", ");
    }
  }
  // Processing Tasks
  printf("\nProcessing Tasks...\n\n");
  Task task = {0};
  while (extract(&ts, &task)) {
    printf(">> Executing Task %u (Priority %u)\n", task.task_id, task.priority);
  }

  free(ts.tasks);
  return EXIT_SUCCESS;
}
//
static bool init_task_scheduler(TaskScheduler *ts, const size_t capacity) {
  if (ts == NULL) {
    fprintf(stderr, "TaskScheduler is NULL.\n");
    return false;
  }
  if (capacity > CAP_MAX) {
    fprintf(stderr, "TaskScheduler capacity exceeds the maximum.\n");
    return false;
  }

  if (capacity == 0) {
    ts->tasks = NULL;
    ts->capacity = 0;
    ts->task_count = 0;
    return true;
  }

  Task *tasks = calloc(capacity, sizeof *tasks);
  if (tasks == NULL) {
    fprintf(stderr, "Failed to allocate tasks.\n");
    return false;
  }
  ts->tasks = tasks;
  ts->capacity = capacity;
  ts->task_count = 0;
  return true;
}
//
static void sift_up(TaskScheduler *ts, size_t child_idx) {
  if (ts == NULL || ts->tasks == NULL) {
    fprintf(stderr, "TaskScheduler is not initialized.\n");
    return;
  }

  while (child_idx > 0) {
    size_t parent_idx = (child_idx - 1) / 2;
    // Check for Min-Heap property
    if (ts->tasks[parent_idx].priority <= ts->tasks[child_idx].priority) {
      break;
    }
    // Keep Sifting Up
    swap(&ts->tasks[parent_idx], &ts->tasks[child_idx]);
    child_idx = parent_idx;
  }
}
//
static void sift_down(TaskScheduler *ts, size_t parent_idx) {
  while (true) {
    size_t left_child_idx = parent_idx * 2 + 1;
    if (left_child_idx >= ts->task_count) {
      return;
    }

    // find the right child idx by comparing left and right children
    size_t min_child_idx = left_child_idx;
    size_t right_child_idx = left_child_idx + 1;
    if (right_child_idx < ts->task_count &&
        ts->tasks[right_child_idx].priority <
            ts->tasks[left_child_idx].priority) {
      min_child_idx = right_child_idx;
    }

    if (ts->tasks[parent_idx].priority <= ts->tasks[min_child_idx].priority) {
      return;
    }

    swap(&ts->tasks[parent_idx], &ts->tasks[min_child_idx]);
    parent_idx = min_child_idx;
  }
}
//
static bool extract(TaskScheduler *ts, Task *task) {
  if (ts == NULL || ts->tasks == NULL || task == NULL || ts->task_count == 0) {
    return false;
  }
  // extract the first task in tasks
  *task = (ts->tasks[0]);

  // Restructure MinHeap
  --(ts->task_count);
  if (ts->task_count > 0) {
    // okay to overwrite since the root task has been extracted beforehand
    ts->tasks[0] = ts->tasks[ts->task_count];
    sift_down(ts, 0);
  }
  return true;
}
//
static bool insert(TaskScheduler *ts, const Task *new_task) {

  if (ts == NULL) {
    fprintf(stderr, "TaskScheduler is NULL.\n");
    return false;
  }

  if (ts->tasks == NULL && ts->capacity > 0) {
    fprintf(stderr, "TaskScheduler is not initialized.\n");
    return false;
  }

  if (new_task == NULL) {
    fprintf(stderr, "Task is NULL.\n");
    return false;
  }

  // CASE: Expand the TaskScheduler capacity by 2x
  if (ts->capacity == ts->task_count) {
    size_t new_capacity;
    if (ts->capacity == 0) {
      new_capacity = 1;
    } else {
      // set max capacity
      if (ts->capacity >= CAP_MAX) {
        fprintf(stderr, "Task Scheduler reached Max Capacity.\n");
        return false;
      }
      // new capacity for task scheduler
      new_capacity = ts->capacity * 2 > CAP_MAX ? CAP_MAX : ts->capacity * 2;
    }
    // Temporary buffer for memory reallocation
    Task *buffer = realloc(ts->tasks, new_capacity * sizeof(*(ts->tasks)));
    if (buffer == NULL) {
      fprintf(stderr, "Failed to reallocate memory for new capacity.\n");
      return false;
    }
    // Successfully reallocated memory for new TaskScheduler capacity
    ts->tasks = buffer;
    ts->capacity = new_capacity;
  }
  // Insert new Task
  size_t new_task_idx = ts->task_count;
  ts->tasks[new_task_idx] = *new_task;
  ++(ts->task_count);
  // Heapify
  sift_up(ts, new_task_idx);
  return true;
}

static void swap(Task *task_x, Task *task_y) {
  if (task_x == NULL || task_y == NULL) {
    fprintf(stderr, "task_x or task_y is NULL.\n");
    return;
  }
  Task temp = *task_x;
  *task_x = *task_y;
  *task_y = temp;
}


````
