#ifndef BITMAP_INDEX_H
#define BITMAP_INDEX_H 1

#include <stdint.h>
#include "thin_portal.h"
#include "map_reduce.h"
#include "gtc.h"

#define NUM_BINS_BITMAP 50
#define NUM_BINS_BITMAP_PLUS_1 51

typedef struct _bin_setting_bitmap
{
    int32_t column_id;
    float min;
    float max;
    float interval;

    // bins array
    uint64_t num_bins;
    float *lower_bounds[NUM_BINS_BITMAP_PLUS_1];
} bin_setting_bitmap;

/*
 * bit vector
 */
typedef struct _bit_vector {
    uint64_t num_records;
    uint64_t length;
    char *bits;
} bit_vector, *bit_vector_p;

/*
 * init data
 */
typedef struct _bitmap_index_init_data {
    uint32_t nparam;
	uint32_t np_total_e;
	uint32_t my_offset_e;
	uint32_t my_num_electrons;
	uint32_t num_columns_indexed;
	float *ptrackede;
    bin_setting_bitmap *bins;
    bit_vector *bit_vectors;
    uint32_t split_chunk_size;  // size of input local chunk each map task processes
    uint32_t next_split_pos; // used by input_splitter
} bitmap_index_init_data, *bitmap_index_init_data_p;

typedef struct _bitmap_index_key
{
    uint32_t column_id;
    uint64_t bin_id;
};

/*
 * per-column index 
 */
typedef struct _column_bitmap_index {
    // phisical index of the indexed column
    uint64_t global_offset;
    uint64_t bound;

    // bit vector for this column
    bit_vector bit_vectors;
} column_bitmap_index;

/*
 * initialize function:
 * setup binning for each coumn to be indexed
 */
int bitmap_index_initialize (data_group *data_grp);

/*
 * input_split function:
 * chunk the input data into pieces
 */
int bitmap_index_input_splitter (void *input_data, 
                                 uint64_t length, 
                                 input_split *splits);

/*
 * map function:
 * generate bit vector chunk for array chunk
 */
int bitmap_index_map (void * data, uint64_t length);

/*
 * partitiona function:
 * distribute keys to staging nodes
 */
int bitmap_index_partition (uint64_t num_reducer, 
                            void *key, 
							uint32_t *node_id);

/*
 * compare_key function:
 * return 0 if two keys are equal; return non-zero value otherwise
 */
int bitmap_index_compare_key(void *key1, void *key2);

/*
 * finalize function:
 * write bitmap indices to files
 */
int bitmap_index_finalize (keys_iterator *keys);

#endif
