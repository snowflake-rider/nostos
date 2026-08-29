# Day 10 — 김현수 원문

출처: https://app.notion.com/e282ae70087182aeaed401784e3363db

Notion 코드 블록의 내용. 아래 코드는 원문 보존용이며 빌드하지 않습니다.

````c
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

/*
- Day 10. 이중 포인터를 활용한 2차원 배열 동적 할당
- **입력:** 행(num_rows) 수, 열(num_cols) 수
- **출력:** 연속된 메모리 공간을 갖는 2차원 배열 구조
- **제약조건:** `malloc` 호출 횟수를 최소화할것 (데이터 영역은 한 번에 할당).
- **실행결과:**

```c
    === Day 10: Dynamic 2D Array Allocation ===

    Generated Matrix (3x4):
     1  2  3  4
     5  6  7  8
     9 10 11 12

    >> Memory successfully freed.
```
*/
static const size_t NUM_ROWS = 3, NUM_COLS = 4;
static int **create_matrix(const size_t num_rows, const size_t num_cols);
static void fill_matrix(int *const *matrix, const size_t num_rows, const size_t num_cols);
static void print_matrix(int *const *matrix, const size_t num_rows, const size_t num_cols);
static void print_row(const int *row, const size_t num_cols, const int width);
static void free_matrix(int **matrix);

int main(void)
{
    int **matrix = create_matrix(NUM_ROWS, NUM_COLS);
    // CASE: Failed to create matrix
    if (matrix == NULL)
    {
        return EXIT_FAILURE; // On this implementation: EXIT_FAILURE == 1
    }
    fill_matrix(matrix, NUM_ROWS, NUM_COLS);
    printf("=== Day 10: Dynamic 2D Array Allocation ===\n\n");
    print_matrix(matrix, NUM_ROWS, NUM_COLS);
    free_matrix(matrix);
    return EXIT_SUCCESS; // On this implementation: EXIT_SUCCESS == 0
}

/**
 * create_matrix: allocates memory for matrix (num_rows x num_cols)
 * - num_rows: number of rows
 * - num_cols: number of columns
 */
static int **create_matrix(const size_t num_rows, const size_t num_cols)
{
    // CHECK: Dimensions
    if (num_rows == 0 || num_cols == 0)
    {
        fprintf(stderr, "Matrix dimensions must be > 0.\n");
        return NULL;
    }

    // STEP 1: Allocate the row-pointer array.
    // matrix is an array of pointers to (array of ints) (matrix: [array_1, array_2, ..., array_n])
    // n == num_rows
    int **matrix = malloc(sizeof(*matrix) * num_rows);
    if (matrix == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for matrix.\n");
        return NULL;
    }

    // STEP 2: Allocate contiguous storage for all matrix values.
    // data: Allocate contiguous storage for num_rows * num_cols integers.
    // n == (num_rows * num_cols)
    int *data = malloc(sizeof(*data) * (num_rows * num_cols));
    if (data == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for data block.\n");
        free(matrix);
        return NULL;
    }

    // STEP 3: Map each matrix row to its starting position in the contiguous data block.
    for (size_t i = 0; i < num_rows; ++i)
    {
        matrix[i] = data + i * num_cols;
    }
    // matrix[0] == data

    return matrix;
}

/**
 * fill_matrix:
 * - Fills the matrix with 1, 2, 3, and so on.
 * - int *const * keeps the row pointers fixed but allows values to change.
 */
static void fill_matrix(int *const *matrix, const size_t num_rows, const size_t num_cols)
{
    for (size_t i = 0; i < num_rows; ++i)
    {
        for (size_t j = 0; j < num_cols; ++j)
        {
            matrix[i][j] = (int)(i * num_cols + j + 1);
        }
    }
}

/**
 * print_matrix
 * - prints the matrix one row at a time.
 */
static void print_matrix(int *const *matrix, const size_t num_rows, const size_t num_cols)
{
    // Width for each element
    size_t max_value = num_rows * num_cols;
    int width = 1;
    while (max_value >= 10)
    {
        max_value /= 10;
        width++;
    }
    printf("Generated Matrix (%zux%zu):\n", num_rows, num_cols);

    for (size_t i = 0; i < num_rows; ++i)
    {
        print_row(matrix[i], num_cols, width);
    }
    putchar('\n');
}

/**
 * print_row:
 * - Prints one row of the matrix.
 * - const int * allows reading the row values but not changing them.
 */
static void print_row(const int *row, const size_t num_cols, const int width)
{
    for (size_t j = 0; j < num_cols; ++j)
    {
        if (j > 0)
        {
            putchar(' ');
        }
        printf("%*d", width, row[j]);
    }
    putchar('\n');
}

/**
 * free_matrix
 * - free the contiguous data block (i.e. matrix[0]) and the row-pointer array (i.e. matrix)
 */
static void free_matrix(int **matrix)
{
    if (matrix == NULL)
    {
        fprintf(stderr, "Trying to free matrix. Matrix points to NULL.\n");
        return;
    }
    // free data block
    free(matrix[0]); // contiguous data block
    // free matrix block
    free(matrix); // row-pointer array
    printf(">> Memory successfully freed.");
}

````
