# Day 16 — 김현수 원문

출처: https://app.notion.com/41a2ae70087182e89be2816d55b6c0de

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// Node
typedef struct Node
{
    int val;
    struct Node *next;
} Node;
// LinkedList
typedef struct LinkedList
{
    Node *head;
} LinkedList;
// LEN: macro for getting length of arr
#define LEN(arr) (sizeof(arr) / sizeof(((arr)[0])))

// Sample data to test with
static LinkedList *sample_ll = NULL;
static const int sample_arr[] = {1, 2, 3};
static const int reversed_sample_arr[] = {3, 2, 1};
// initialize LinkedList
static LinkedList *init(const int arr[], size_t len);
// destory the LinkedList(itself and nodes linked)
static void destroy(LinkedList *ll);
// print LinkedList's head to tail (NULL)
static void print(const LinkedList *ll);
// reverse the links of LinkedList
static void reverse(LinkedList *ll);
// check if linked list ll matches the arr
static bool ll_equals_array(const LinkedList *ll, const int arr[], size_t len);
int main(void)
{
    // Original Linked List
    sample_ll = init(sample_arr, LEN(sample_arr));
    if (sample_ll == NULL)
    {
        return EXIT_FAILURE;
    }

    print(sample_ll);

    // Reversed Linked List
    reverse(sample_ll);
    print(sample_ll);

    // ❌ Reversal Verification
    if (!ll_equals_array(sample_ll, reversed_sample_arr, LEN(reversed_sample_arr)))
    {
        fprintf(stderr, "LinkedList failed to reverse. \n");
        destroy(sample_ll);
        sample_ll = NULL;
        return EXIT_FAILURE;
    }

    // ✅ Linked List Verified to be reversed.
    printf("List Reversed.\n");
    destroy(sample_ll);
    sample_ll = NULL;

    return EXIT_SUCCESS;
}

static LinkedList *init(const int arr[], size_t len)
{
    // Reject when arr is NULL and len > 0
    if (arr == NULL && len > 0)
    {
        return NULL;
    }
    // 1. Allocate memory for LinkedList
    LinkedList *ll = malloc(sizeof(*ll));
    if (ll == NULL)
    {
        return NULL;
    }
    ll->head = NULL;
    Node *tail = NULL;
    // 2. Construct Linked List from arr
    for (size_t i = 0; i < len; i++)
    {
        Node *node = malloc(sizeof(*node));
        if (node == NULL)
        {
            fprintf(stderr, "NODE ALLOCATION FAILED.\n");
            destroy(ll);
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

static void destroy(LinkedList *ll)
{
    if (ll == NULL)
    {
        return;
    }
    // 1. Free Nodes (head to tail)
    Node *curr = ll->head;
    while (curr != NULL)
    {
        Node *next = curr->next;
        free(curr);
        curr = next;
    }
    // 2. Free LinkedList
    free(ll);
}

static void print(const LinkedList *ll)
{
    if (ll == NULL)
    {
        return;
    }
    const Node *curr = ll->head;
    while (curr != NULL)
    {
        printf("%d -> ", curr->val);
        curr = curr->next;
    }
    printf("NULL\n");
}

static void reverse(LinkedList *ll)
{
    if (ll == NULL || ll->head == NULL)
    {
        return;
    }
    //
    Node *prev = NULL;
    Node *curr = ll->head;

    while (curr != NULL)
    {
        Node *next = curr->next; // 1. save next node (before losing it)
        curr->next = prev;       // 2. reverse the curr link
        prev = curr;             // 3. advance prev
        curr = next;             // 3. advance curr
    }
    ll->head = prev;
}

static bool ll_equals_array(const LinkedList *ll, const int arr[], size_t len)
{
    if (ll == NULL || (arr == NULL && len > 0))
    {
        return false;
    }
    const Node *curr = ll->head;
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

````
