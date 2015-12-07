#include "mpi.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "histogram.h"

/*
 * set up uniform bin boundaries for histogram
 */
int setup_uniform_binning(bin_setting *bins, int column_id, double min, double max, double num_bins)
{
    bins->num_bins = num_bins;
    bins->min = min;
    bins->max = max;
    if(!bins->lower_bounds) {
        bins->lower_bounds = (double *) malloc((num_bins+1) * sizeof(double));
        if(!bins->lower_bounds) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
    }

    // get boundaries of bins
    bins->interval = (bins->max - bins->min) / bins->num_bins;
    int k;
    for(k = 0; k < bins->num_bins; k ++) {
        bins->lower_bounds[k] = bins->min + bins->interval * k;
//fprintf(stderr, "bin boundary: id=%d k=%d lowerbound=%f\n", column_id,k,  bins->lower_bounds[k]);
    }
    bins->lower_bounds[bins->num_bins] = bins->max;
    return 0;
}

/*
 * initialize histogram
 */
int initialize_hostogram(histogram *h, int column_id, double min, double max, int num_bins) 
{
    h->column_id = column_id;
    h->num_bins = num_bins;
    int rc = setup_uniform_binning(&h->bins, column_id, min, max, num_bins);
    if(rc) { 
        return rc;
    } 
    if(!h->count) {
        h->count = (uint64_t *) calloc(num_bins, sizeof(uint64_t));
        if(!h->count) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
    }
    else {
        memset(h->count, 0, num_bins * sizeof(uint64_t));
    }
    return 0;
}


void free_histogram(histogram *h)
{
    free(h->bins.lower_bounds);
    free(h->count);
}

uint64_t find_bin(double value, histogram *h)
{
    uint64_t idx = (uint64_t)((value - h->bins.min) / h->bins.interval);
//fprintf(stderr, "idx=%lu value=%f colid=%lu bins[]min=%f binsmax=%f interval=%f %s:%d\n", idx, value, col_id, bins[col_id].min, bins[col_id].max,bins[col_id].interval,__FILE__, __LINE__);
    if(idx == h->bins.num_bins) idx --;
    return idx;
}

void update_count(double value, histogram *h)
{
    uint64_t idx = (uint64_t)((value - h->bins.min) / h->bins.interval);
    if(idx == h->bins.num_bins) idx --;
    h->count[idx] ++;
}


int initialize_hostogram_2d(histogram_2d *h, int a1_id, int a2_id, int a1_num_bins, int a2_num_bins)
{
    h->a1_id = a1_id;
    h->a2_id = a2_id;
    h->a1_num_bins = a1_num_bins;
    h->a2_num_bins = a2_num_bins;
    if(!h->count) {
        h->count = (uint64_t *) calloc(a1_num_bins * a2_num_bins, sizeof(uint64_t));
        if(!h->count) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
    }
    else {
        memset(h->count, 0, a1_num_bins * a2_num_bins * sizeof(uint64_t));
    }
    return 0;
}

void update_count_2d(double a1_value, double a2_value, histogram_2d *h)
{
    uint64_t a1_idx = (uint64_t)((a1_value - h->a1_bins->min) / h->a1_bins->interval);
    if(a1_idx >= h->a1_num_bins) a1_idx = h->a1_num_bins - 1;
    uint64_t a2_idx = (uint64_t)((a2_value - h->a2_bins->min) / h->a2_bins->interval);
    if(a2_idx >= h->a2_num_bins) a2_idx = h->a2_num_bins - 1;
    h->count[a1_idx * h->a2_num_bins + a2_idx] ++;
}

void free_histogram_2d(histogram_2d *h)
{
    if(h->count)
        free(h->count);
    if(h->a1_bins->lower_bounds)
        free(h->a1_bins->lower_bounds);
    if(h->a2_bins->lower_bounds)
        free(h->a2_bins->lower_bounds);
}


