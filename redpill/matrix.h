/* 김현수, Redpill Day 10. See README.md for contracts and adaptations. */
#ifndef REDPILL_MATRIX_H
#define REDPILL_MATRIX_H

#include <stdbool.h>

#include <stddef.h>

#include <stdint.h>

int **rp10_create_matrix(const size_t num_rows, const size_t num_cols);

void rp10_free_matrix(int **matrix);

int rp10_demo(void);

#endif
