# Day 11 — 김현수 원문

출처: https://app.notion.com/4872ae70087182668ee881d69ba26ad7

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>

// enum Operation
// OP_ADD: 0 ... OP_EXIT: 4
typedef enum
{
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EXIT
} Operation;

// OperationFunction is a pointer-to-function type.
// Parameters: two pointers to const int
// Return value: int
typedef int (*OperationFunction)(const int *a, const int *b);
static int add(const int *a, const int *b);
static int sub(const int *a, const int *b);
static int mul(const int *a, const int *b);
static int div(const int *a, const int *b);
static void discard_input_line(void);
static Operation interact_with_menu(void);

int main(void)
{
    static const OperationFunction operations[4] = {
        add, sub, mul, div};
    Operation op;
    int x, y;
    printf("=== Day 11: Function Pointer Array Calculator ===\n\n");
    while ((op = interact_with_menu()) != OP_EXIT)
    {
        printf("Input two integers: ");
        // Require two integers and reject zero as a divisor.
        while (scanf("%d %d", &x, &y) != 2 || (op == OP_DIV && y == 0))
        {
            discard_input_line();
            printf("Invalid input or zero divisor.\n");
            printf("Input two integers: ");
        }
        // Remove the newline or anything after the first two integers.
        discard_input_line();

        printf(">> Result: %d\n\n", operations[op](&x, &y));
    }
    return 0;
}
// Discard the rest of the invalid line.
// Stop when a newline or end-of-file is reached.
static void discard_input_line(void)
{
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF)
    {
    }
}
static Operation interact_with_menu(void)
{
    int input;
    printf("Select (0:Add, 1:Sub, 2:Mul, 3:Div, 4:Exit): ");
    while (scanf("%d", &input) != 1 || (input < OP_ADD) || (input > OP_EXIT))
    {
        discard_input_line();
        printf("Please choose between 0 and 4.\n");
        printf("Select (0:Add, 1:Sub, 2:Mul, 3:Div, 4:Exit): ");
    }
    discard_input_line();

    return (Operation)input;
}
static int add(const int *a, const int *b)
{
    return *a + *b;
}
static int sub(const int *a, const int *b)
{
    return *a - *b;
}
static int mul(const int *a, const int *b)
{
    return (*a) * (*b);
}
static int div(const int *a, const int *b)
{
    return (*a) / (*b);
}


````
