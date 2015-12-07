#ifndef _PREDATA_TRANSPOSE_H_
#define _PREDATA_TRANSPOSE_H_
/*
 * Transpose 2D array
 *
 * NOTE: assume C-style rwo-majored memory layout
 */

/*
 * initialize by allocating array to hold result
 */
int initialize_transpose(int row_size, int column_size, double **result_buf);


/*
 * finalize by de-allocating result array
 */
void finalize_transpose(double *t);

/*
 * transpose original[rwo_size][column_size] to result[column_size][two_size]
 */
void do_transpose(double *original, double *result, int row_size, int column_size);

/*
 * finalize
 */
void finalize_transpose(double *result);

#endif

