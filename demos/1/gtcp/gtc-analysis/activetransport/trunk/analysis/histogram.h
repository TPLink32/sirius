#ifndef _PREDATA_HISTOGRAM_H_
#define _PREDATA_HISTOGRAM_H_ 
/*
 * calculate histogram on gts particle data
 *
 */
#include <stdint.h>

typedef struct _bin_setting
{
    int num_bins;         // number of bin buckets    
    double min;           // global minimum of this attribute
    double max;           // global maximum of this attribute 
    double interval;      // bin size
    double *lower_bounds; // breaking points (lower bounds)
} bin_setting;

/*
 * 1D histogram
 */
typedef struct _histogram
{
    int column_id;        // for attribute 1-7
    int num_bins;         // number of bin buckets
    bin_setting bins;     // bind boundaries
    uint64_t *count;      // counter values
} histogram, *histogram_t;

/*
 * 2D histogram
 */
typedef struct _histogram_2d
{
    int a1_id;            // attribute 1 
    int a2_id;            // attribute 2
    int a1_num_bins;      // number of bin buckets for attr1
    int a2_num_bins;      // number of bin buckets for attr2
    bin_setting *a1_bins; // bind boundaries for attribute 1
    bin_setting *a2_bins; // bind boundaries for attribute 2
    uint64_t *count;      // counter values
} histogram_2d, *histogram_2d_t;


int setup_uniform_binning(bin_setting *bins, int column_id, double min, double max, double num_bins);

int initialize_hostogram(histogram *h, int column_id, double min, double max, int num_bins);

void free_histogram(histogram *h);

uint64_t find_bin(double value, histogram *h);

void update_count(double value, histogram *h);

int initialize_hostogram_2d(histogram_2d *h, int a1_id, int a2_id, int a1_num_bins, int a2_num_bins);

void update_count_2d(double a1_value, double a2_value, histogram_2d *h);

void free_histogram_2d(histogram_2d *h);

#endif
