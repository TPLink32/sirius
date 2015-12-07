#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "mpi.h"
#include "thin_portal.h"
#include "map_reduce.h"
#include "gtc.h"
#include "bitmap_index.h"

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
int bitmap_index_initialize (data_group *data_grp)
{
    uint32_t num_workers = get_num_workers();  // for this operation instance

	bitmap_index_init_data *b_init_data = (bitmap_index_init_data *) 
        malloc(sizeof(bitmap_index_init_data));
    if(!b_init_data) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return -1;
    }
    
    // we only index the first column, i.e. radius coordinate
    b_init_data->num_columns_indexed = 1;
    bin_setting *bins = (bin_setting *) 
        malloc(sizeof(bin_setting) * b_init_data->num_columns_indexed);
    if(!bins) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return -1;
    }
    b_init_data->bins = bins;

    int32_t i;
    for(i = 0; i < b_init_data->num_columns_indexed; i ++) {
        bins->column_id = i;
      
        // TODO: generate global min/max, A BIG PROBLEM! 
        bins[i].min = data_grp->characteristics->ptrackede_min[i];
        bins[i].max = data_grp->characteristics->ptrackede_min[i];

        // TODO: bins distributed evenly by default 
        bins[i].num_bins = NUM_BINS_BITMAP;
        
        // get boundaries of bins
        bins[i].interval = (bins[i].max - bins[i].min) / bins[i].num_bins;
        uint64_t k;
        for(k = 0; k < bins[i].num_bins; k ++) {
            bins[i].lower_bounds[k] = bins[i].min + bins[i].interval * k;
        }
        bins[i].lower_bounds[bins[i].num_bins] = bins[i].max;    
    }
	
	// TODO: access input data
	gtc_electrons *input_data = (gtc_electrons *) data_grp->input_data;
    b_init_data->nparam = input_data->nparam; 
    b_init_data->my_offset_e = input_data->my_offset_e; 
    b_init_data->np_total_e = input_data->np_total_e; 
    b_init_data->my_num_electrons = input_data->my_num_electrons; 
    b_init_data->ptrackede = input_data->ptrackede; 

    // TODO: correct it: per column per bin
	b_init_data->bit_vectors = (bit_vector *) 
		malloc(sizeof(bit_vector) * b_init_data->num_columns_indexed);
    if(!b_init_data->bit_vectors) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", 
			__FILE__, __LINE__);
        return -1;
    }
    for(i = 0; i < b_init_data->num_columns_indexed; i ++) {
		b_init_data->bit_vectors[i].num_records = 
			b_init_data->my_num_electrons;
		b_init_data->bit_vectors[i].length = 
			b_init_data->bit_vectors[i].num_records / 8;
		if(b_init_data->bit_vectors[i].num_records / 8) {
            b_init_data->bit_vectors[i].length ++;
		}
		b_init_data->bit_vectors[i].bits = (char *) 
			malloc(b_init_data->bit_vectors[i].length);
		if(!b_init_data->bit_vectors[i].bits) {
			fprintf(stderr, "cannot allocate memory. %s:%d\n", 
				__FILE__, __LINE__);
			return -1;
		}
        memset(b_init_data->bit_vectors[i].bits, 0, 
			b_init_data->bit_vectors[i].length); 
	}
  
    // make it modulo of 8 so we don't need to worry about byte-alignment
    b_init_data->split_chunk_size = 
		b_init_data->my_num_electrons / num_workers;
	if(b_init_data->split_chunk_size % 8) {
        b_init_data->split_chunk_size += 
			(8 - b_init_data->split_chunk_size % 8); 
	}
    b_init_data->next_split_pos = 0;

    register_init_data(b_init_data);
    return 0;
}

/*
 * input_split function:
 * chunk the input data into pieces
 */
int bitmap_index_input_splitter (void *input_data, 
                                 uint64_t length, 
                                 input_split *splits)
{
    assert(input_data);
    assert(split);

    bitmap_index_init_data *b_init_data = 
        (bitmap_index_init_data *) get_init_data();

    gtc_electrons *electrons = (gtc_electrons *)input_data;

    if(b_init_data->next_split_pos < electrons->my_num_electrons) {
        splits->data = electrons->ptrackede[b_init_data->next_split_pos];
        if((b_init_data->next_split_pos + b_init_data->split_chunk_size) >= 
            electrons->my_num_electrons) {
			// the last piece can be larger than normal ones
            splits->length = electrons->my_num_electrons - 
                b_init_data->next_split_pos;
        }
        else {
            splits->length = b_init_data->split_chunk_size;
        }
        b_init_data->next_split_pos += splits->length;  
        return 0;
    }
    else {
        // we have already reached the end of array
        return -1;
    }
}

/*
 * map function:
 * generate bit vector chunk for array chunk
 */
int bitmap_index_map (void * data, uint64_t length)
{
    bitmap_index_init_data *b_init_data = 
        (bitmap_index_init_data *) get_init_data();

    uint32_t nparam = b_init_data->nparam;

	float *ptrackede = (float *) data;
    uint32_t i, j;
    for(j = 0; j < length; j ++) {
        for(i = 0; i < b_init_data->num_columns_indexed; i ++) {
            uint32_t column_id = b_init_data->bins[i].column_id;
            
			// find out which bin this record falls into
			uint64_t bin_idx;
			bin_idx = (uint64_t)((ptrackede[j*nparam+column_id] - 
				b_init_data->bins[i].min) / b_init_data->bins[i].interval);

            if(bin_idx == b_init_data->bins[i].num_bins) {
				bin_idx --;
			}

            char *byte_to_change = 
				&(b_init_data->bit_vectors[i].bits[bin_idx/8]);
            *byte_to_change |= (0x80 >> (bin_idx%8));
        }
    }
	// TODO: nothing to emit

    return 0;
}

/*
 * partitiona function:
 * distribute keys to staging nodes
 */
int bitmap_index_partition (uint64_t num_reducer, void *key, uint32_t *node_id)
{
	// do not exchange information among staging nodes
    *node_id = get_staging_node_rank(); 
	return 0;
}

/*
 * compare_key function:
 * return 0 if two keys are equal; return non-zero value otherwise
 */
int bitmap_index_compare_key(void *key1, void *key2)
{
    return 0;
}

/*
 * finalize function:
 * write bitmap indices to files
 */
int bitmap_index_finalize (keys_iterator *keys)
{
    bitmap_index_init_data *b_init_data = 
        (bitmap_index_init_data *) get_init_data();

	uint32_t i, j;
    for(i = 0; i < b_init_data->num_columns_indexed; i ++) {
        int32_t column_id = b_init_data->bins[i].column_id;
		char f_name[70];
		sprintf(f_name, "trackp_dir/b_index_electron_%.5d_%.d\0", 
			column_id, iteration_no);
		int b_file = open(f_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if(b_file < 0) {
			fprintf(stderr, "cannot open file %s %s:%d\n", 
				f_name, __FILE__, __LINE__);
			return -1;
		}
    
		for(j = 0; j < NUM_BINS_BITMAP; j ++) {
			size_t rc = write(b_file, b_init_data->bit_vectors[i].bits, 
				b_init_data->bit_vectors[i].length);
		}

		close(b_file);
	}

    // free user-provided memory
	free(b_init_data->bins);
    for(i = 0; i < b_init_data->num_columns_indexed; i ++) {
		free(b_init_data->bit_vectors[i].bits);
	}
	free(b_init_data->bit_vectors);

	// we don't free it until finishing all operaqtions for the last io dump
	if(is_last_io_dump()) {
        free(b_init_data->ptrackede);
    }
	
	return 0;
}
