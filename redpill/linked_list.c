/* 김현수 제출 코드 기반. 원문: originals/day16.md */
#include "linked_list.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// Node

// LinkedList

// LEN: macro for getting length of arr
#define LEN(arr) (sizeof(arr) / sizeof(((arr)[0])))

// Sample data to test with
static rp16_LinkedList *sample_ll = NULL;
static const int sample_arr[] = {1, 2, 3};
static const int reversed_sample_arr[] = {3, 2, 1};
// initialize LinkedList
rp16_LinkedList *rp16_init(const int arr[], size_t len);
// destory the LinkedList(itself and nodes linked)
void rp16_destroy(rp16_LinkedList *ll);
// print LinkedList's head to tail (NULL)
static void print(const rp16_LinkedList *ll);
// reverse the links of LinkedList
void rp16_reverse(rp16_LinkedList *ll);
// check if linked list ll matches the arr
bool rp16_ll_equals_array(const rp16_LinkedList *ll, const int arr[], size_t len);
int rp16_demo(void)
{
    // Original Linked List
    sample_ll = rp16_init(sample_arr, LEN(sample_arr));
    if (sample_ll == NULL)
    {
        return EXIT_FAILURE;
    }

    print(sample_ll);

    // Reversed Linked List
    rp16_reverse(sample_ll);
    print(sample_ll);

    // ❌ Reversal Verification
    if (!rp16_ll_equals_array(sample_ll, reversed_sample_arr, LEN(reversed_sample_arr)))
    {
        fprintf(stderr, "LinkedList failed to reverse. \n");
        rp16_destroy(sample_ll);
        sample_ll = NULL;
        return EXIT_FAILURE;
    }

    // ✅ Linked List Verified to be reversed.
    printf("List Reversed.\n");
    rp16_destroy(sample_ll);
    sample_ll = NULL;

    return EXIT_SUCCESS;
}

rp16_LinkedList *rp16_init(const int arr[], size_t len)
{
    // Reject when arr is NULL and len > 0
    if (arr == NULL && len > 0)
    {
        return NULL;
    }
    // 1. Allocate memory for LinkedList
    rp16_LinkedList *ll = malloc(sizeof(*ll));
    if (ll == NULL)
    {
        return NULL;
    }
    ll->head = NULL;
    rp16_Node *tail = NULL;
    // 2. Construct Linked List from arr
    for (size_t i = 0; i < len; i++)
    {
        rp16_Node *node = malloc(sizeof(*node));
        if (node == NULL)
        {
            fprintf(stderr, "NODE ALLOCATION FAILED.\n");
            rp16_destroy(ll);
            return NULL;
        }
        node->val = arr[i];
        node->next = NULL;

        if (ll->head == NULL)
        {
            ll->head = node;
        }
        else
        {
            tail->next = node;
        }
        tail = node;
    }
    return ll;
}

void rp16_destroy(rp16_LinkedList *ll)
{
    if (ll == NULL)
    {
        return;
    }
    // 1. Free Nodes (head to tail)
    rp16_Node *curr = ll->head;
    while (curr != NULL)
    {
        rp16_Node *next = curr->next;
        free(curr);
        curr = next;
    }
    // 2. Free LinkedList
    free(ll);
}

static void print(const rp16_LinkedList *ll)
{
    if (ll == NULL)
    {
        return;
    }
    const rp16_Node *curr = ll->head;
    while (curr != NULL)
    {
        printf("%d -> ", curr->val);
        curr = curr->next;
    }
    printf("NULL\n");
}

void rp16_reverse(rp16_LinkedList *ll)
{
    if (ll == NULL || ll->head == NULL)
    {
        return;
    }
    //
    rp16_Node *prev = NULL;
    rp16_Node *curr = ll->head;

    while (curr != NULL)
    {
        rp16_Node *next = curr->next; // 1. save next node (before losing it)
        curr->next = prev;       // 2. reverse the curr link
        prev = curr;             // 3. advance prev
        curr = next;             // 3. advance curr
    }
    ll->head = prev;
}

bool rp16_ll_equals_array(const rp16_LinkedList *ll, const int arr[], size_t len)
{
    if (ll == NULL || (arr == NULL && len > 0))
    {
        return false;
    }
    const rp16_Node *curr = ll->head;
    for (size_t i = 0; i < len; i++)
    {
        if (curr == NULL || curr->val != arr[i])
        {
            return false;
        }
        curr = curr->next;
    }
    // if curr != NULL, then extra Node exists. -> unequal
    return curr == NULL;
}
