#include <stdio.h>
#include <stdlib.h>
#include "transpose.h"

/*
 * initialize
 */
int initialize_transpose(int row_size, int column_size, double **result_buf) 
{
    *result_buf = (double *) malloc(row_size * column_size * sizeof(double));
    if(!*result_buf) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return -1;
    }
    return 0;
}

/*
 * finalize 
 */
void free_transpose(double *t)
{
    free(t);
}

/*
 * transpose original[rwo_size][column_size] to result[column_size][two_size]
 */
void do_transpose(double *original, double *result, int row_size, int column_size)
{
    int i, j;
    for(i = 0; i < row_size; i ++) {
        for(j = 0; j < column_size; j ++) {
            result[i + row_size * j] = original[i * column_size + j]; 
        }
    }
}

/*
 * finalize
 */
void finalize_transpose(double *result)
{
    free(result);
}
