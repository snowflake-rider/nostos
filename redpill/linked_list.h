/* 김현수, Redpill Day 16. See README.md for contracts and adaptations. */
#ifndef REDPILL_LINKED_LIST_H
#define REDPILL_LINKED_LIST_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

typedef struct rp16_Node
{
    int val;
    struct rp16_Node *next;
} rp16_Node;

typedef struct rp16_LinkedList
{
    rp16_Node *head;
} rp16_LinkedList;

rp16_LinkedList *rp16_init(const int arr[], size_t len);

void rp16_destroy(rp16_LinkedList *ll);

void rp16_reverse(rp16_LinkedList *ll);

bool rp16_ll_equals_array(const rp16_LinkedList *ll, const int arr[], size_t len);

int rp16_demo(void);

#endif
