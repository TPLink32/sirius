/* 
 * This program performs analysis (mainly range query, histogram, and transpose)
 * on GTS particle output data. It runs as a MPI program on staging nodes
 * Particle data is transferred through IBPBIO transport.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <mpi.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

// IB transport
//#include "read_ib.h"
#include "adios_read.h"
#include "adios_error.h"

// analysis specific
#include <math.h>
#include "Analyp.h"
#include "histogram.h"

// for timing
#include "adios_read_timing.h"
#include "adios_sim_timing.h"

// for thread pinning
//#include "thread_pin.h"
#include <sched.h>

#define DEBUG_OUTPUT 1
#define XPMEM_READ 1

// global variable to carry state cross timesteps
long int my_mi;
double *qu_mi = NULL;
long int my_me;
double *qu_me = NULL;

struct timestep_data
{
    int32_t mi;
    double * zion;

    int32_t me;
    double * zeon;
};

double *transpose_buf;
int first_time_transpose = 1;

/////////////////////////////////////////////////////////////
//                   Range Qurery                          //
/////////////////////////////////////////////////////////////
int range_query_analysis(gts_global *gts_global_features,
                         struct timestep_data * pgs,
                         int num_pgs, 
                         int is_zion,                            
                         int rank,
                         MPI_Comm comm
                        )
{
    int numtasks;
    MPI_Comm_size(comm, &numtasks);

    long int num_mi = 0; // reset to 0 every timestep
    double **qu;
    if(is_zion) {    
        qu = &qu_mi;
    }    
    else {
        qu = &qu_me;
    }
    if(!*qu) {
        *qu = (double *) malloc(sizeof(double)*PG_NUM*7);
        if(!*qu) {

            size_t avail_pgs =  sysconf (_SC_AVPHYS_PAGES);

            fprintf(stderr, "cannot allocate memory. %d %lu %lu %s:%d\n", is_zion, avail_pgs, avail_pgs*4096,__FILE__,__LINE__);
            return -1; 
        }
    }

    // calculate global avg and stdev in order to form range query creteria
    
    /* Averages for X, Y, Z*/
    double G_avg_v1, G_avg_w, G_avg_v2;
    G_avg_v1 = gts_global_features->mi_sum_sqr_v1 / gts_global_features->mi_gcnt;
    G_avg_w = gts_global_features->mi_sum_sqr_w / gts_global_features->mi_gcnt;
    G_avg_v2 = gts_global_features->mi_sum_sqr_v2 / gts_global_features->mi_gcnt;

#if PRINT_RESULT
    printf("\n avg value %lf , %lf , %lf | %lf , %lf , %lf \n", gts_global_features->mi_sum_sqr_v1, gts_global_features->mi_sum_sqr_w, gts_global_features->mi_sum_sqr_v2, G_avg_v1, G_avg_w, G_avg_v2);
#endif

    /* Standard deviation for X, Y, Z */
    double G_stdev_v1, G_stdev_w, G_stdev_v2;
    G_stdev_v1 = sqrt((gts_global_features->mi_sqr_sum_v1/gts_global_features->mi_gcnt)-(G_avg_v1*G_avg_v1));
    G_stdev_w = sqrt((gts_global_features->mi_sqr_sum_w/gts_global_features->mi_gcnt)-(G_avg_w*G_avg_w));
    G_stdev_v2 = sqrt((gts_global_features->mi_sqr_sum_v2/gts_global_features->mi_gcnt)-(G_avg_v2*G_avg_v2));

    /* Upper and lower limit for X, Y, Z */
    double Uplimit[3], Lolimit[3];
    Lolimit[0] = G_avg_v1 - G_stdev_v1 / 4;
    Uplimit[0] = G_avg_v1 + G_stdev_v1 / 4;
    Lolimit[1] = G_avg_w - G_stdev_w;
    Uplimit[1] = G_avg_w + G_stdev_w;
    Lolimit[2] = G_avg_v2 - G_stdev_v2;
    Uplimit[2] = G_avg_v2 + G_stdev_v2;

#if PRINT_RESULT
    printf("\nstdev value  %lf , %lf, %lf \n", G_stdev_v1, G_stdev_w, G_stdev_v2);
#endif

    int i;
    double *zion;
    int mi;
    for(i = 0; i < num_pgs; i ++) {        
//fprintf(stderr, "i=%d %s:%d\n", i,__FILE__,__LINE__);
        if(is_zion) {    
            zion = pgs [i].zion;
            mi = pgs [i].mi;
        }
        else {
            zion = pgs [i].zeon;
            mi = pgs [i].me;
        }

//fprintf(stderr, "i=%d mi=%d\n",i, *mi);
        // filter local array and pack filtered particles to qu_mi
        Scanfilter_combine(zion, mi, *qu, &num_mi, Lolimit, Uplimit);
//fprintf(stderr, "i=%d mi=%d num_mi=%ld\n",i, *mi, num_mi);

    }

    // finalize for this timestep
    if(is_zion) {
        my_mi = num_mi;
    }
    else {
        my_me = num_mi;
    }

#if PRINT_RESULT
        for(i = 0; i < num_mi*7; i++){
            if((i % 7) == 0) printf("\n");
            printf("%lf ", (*qu)[i]);
        }
        printf("\n");
        printf ("Number of tasks= %d My rank= %d\n", numtasks,rank);
        printf("--------------------------------------\n");
#endif


    return 0;
}
        
/////////////////////////////////////////////////////////////
//             End of Range Qurery                         //
/////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////
//             Global Min/Max/Sum/Sqr_sum                  //
/////////////////////////////////////////////////////////////

int global_minmax(gts_local *gts_local_features,  
                  gts_global *gts_global_features,
                  MPI_Comm comm
                 )
{
    /* reduce the features of X and distribute to all nodes */
    MPI_Allreduce(&(gts_local_features->mi_cnt), &(gts_global_features->mi_gcnt), 1, MPI_LONG, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_min_x), &(gts_global_features->mi_gmin_x), 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(&(gts_local_features->mi_max_x), &(gts_global_features->mi_gmax_x), 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(&(gts_local_features->mi_sqr_sum_x), &(gts_global_features->mi_sqr_sum_x), 1, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_sum_x), &(gts_global_features->mi_sum_sqr_x), 1, MPI_DOUBLE, MPI_SUM, comm);


#if PRINT_RESULT
    printf("\nHong Fang X : %ld , %d , %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
            gts_local_features->mi_cnt, gts_global_features->mi_gcnt,
            gts_local_features->mi_min_x, gts_global_features->mi_gmin_x,
            gts_local_features->mi_max_x, gts_global_features->mi_gmax_x,
            gts_local_features->mi_sqr_sum_x, gts_global_features->mi_sqr_sum_x,
            gts_local_features->mi_sum_x, gts_global_features->mi_sum_sqr_x
            );
#endif
        
    /* reduce the features of Y and distribute to all nodes */
    MPI_Allreduce(&(gts_local_features->mi_min_y), &(gts_global_features->mi_gmin_y), 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(&(gts_local_features->mi_max_y), &(gts_global_features->mi_gmax_y), 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(&(gts_local_features->mi_sqr_sum_y), &(gts_global_features->mi_sqr_sum_y), 1, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_sum_y), &(gts_global_features->mi_sum_sqr_y), 1, MPI_DOUBLE, MPI_SUM, comm);

#if PRINT_RESULT
    printf("\nHong Fang Y : %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
            gts_local_features->mi_min_y, gts_global_features->mi_gmin_y,
            gts_local_features->mi_max_y, gts_global_features->mi_gmax_y,
            gts_local_features->mi_sqr_sum_y, gts_global_features->mi_sqr_sum_y,
            gts_local_features->mi_sum_y, gts_global_features->mi_sum_sqr_y
            );
#endif

    /* reduce the features of Z and distribute to all nodes */
    MPI_Allreduce(&(gts_local_features->mi_min_z), &(gts_global_features->mi_gmin_z), 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(&(gts_local_features->mi_max_z), &(gts_global_features->mi_gmax_z), 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(&(gts_local_features->mi_sqr_sum_z), &(gts_global_features->mi_sqr_sum_z), 1, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_sum_z), &(gts_global_features->mi_sum_sqr_z), 1, MPI_DOUBLE, MPI_SUM, comm);

#if PRINT_RESULT
    printf("\nHong Fang Z : %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
            gts_local_features->mi_min_z, gts_global_features->mi_gmin_z,
            gts_local_features->mi_max_z, gts_global_features->mi_gmax_z,
            gts_local_features->mi_sqr_sum_z, gts_global_features->mi_sqr_sum_z,
            gts_local_features->mi_sum_z, gts_global_features->mi_sum_sqr_z
            );
#endif

    /* reduce the features of V1 and distribute to all nodes */
    MPI_Allreduce(&(gts_local_features->mi_min_v1), &(gts_global_features->mi_gmin_v1), 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(&(gts_local_features->mi_max_v1), &(gts_global_features->mi_gmax_v1), 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(&(gts_local_features->mi_sqr_sum_v1), &(gts_global_features->mi_sqr_sum_v1), 1, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_sum_v1), &(gts_global_features->mi_sum_sqr_v1), 1, MPI_DOUBLE, MPI_SUM, comm);

#if PRINT_RESULT
    printf("\nHong Fang v1 : %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
            gts_local_features->mi_min_v1, gts_global_features->mi_gmin_v1,
            gts_local_features->mi_max_v1, gts_global_features->mi_gmax_v1,
            gts_local_features->mi_sqr_sum_v1, gts_global_features->mi_sqr_sum_v1,
            gts_local_features->mi_sum_v1, gts_global_features->mi_sum_sqr_v1
            );
#endif

    /* reduce the features of W and distribute to all nodes */
    MPI_Allreduce(&(gts_local_features->mi_min_w), &(gts_global_features->mi_gmin_w), 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(&(gts_local_features->mi_max_w), &(gts_global_features->mi_gmax_w), 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(&(gts_local_features->mi_sqr_sum_w), &(gts_global_features->mi_sqr_sum_w), 1, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_sum_w), &(gts_global_features->mi_sum_sqr_w), 1, MPI_DOUBLE, MPI_SUM, comm);

#if PRINT_RESULT
    printf("\nHong Fang w : %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
            gts_local_features->mi_min_w, gts_global_features->mi_gmin_w,
            gts_local_features->mi_max_w, gts_global_features->mi_gmax_w,
            gts_local_features->mi_sqr_sum_w, gts_global_features->mi_sqr_sum_w,
            gts_local_features->mi_sum_w, gts_global_features->mi_sum_sqr_w
            );
#endif


    /* reduce the features of V2 and distribute to all nodes */
    MPI_Allreduce(&(gts_local_features->mi_min_v2), &(gts_global_features->mi_gmin_v2), 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(&(gts_local_features->mi_max_v2), &(gts_global_features->mi_gmax_v2), 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(&(gts_local_features->mi_sqr_sum_v2), &(gts_global_features->mi_sqr_sum_v2), 1, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_sum_v2), &(gts_global_features->mi_sum_sqr_v2), 1, MPI_DOUBLE, MPI_SUM, comm);

#if PRINT_RESULT
    printf("\nHong Fang v2 : %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
            gts_local_features->mi_min_v2, gts_global_features->mi_gmin_v2,
            gts_local_features->mi_max_v2, gts_global_features->mi_gmax_v2,
            gts_local_features->mi_sqr_sum_v2, gts_global_features->mi_sqr_sum_v2,
            gts_local_features->mi_sum_v2, gts_global_features->mi_sum_sqr_v2
            );
#endif


    return 0;
}

/////////////////////////////////////////////////////////////
//             End of global Min/Max/Sum/Sqr_sum           //
/////////////////////////////////////////////////////////////

        
/////////////////////////////////////////////////////////////
//             Histogram                                   //
/////////////////////////////////////////////////////////////

#define NUM_BINS 1000

typedef struct _histogram_data
{
    histogram local_histograms[6];
    histogram global_histograms[6];
} histogram_data;

histogram_data histograms;
histogram_data histograms_e;

/*
 * histogram per-timestep initialization
 */
int histogram_analysis_init(gts_local *gts_local_features, 
                            gts_global *gts_global_features,
                            histogram_data *h,
                            int rank,
                            int first_time 
                           )
{
    int i, rc;

    // TODO: move to initialization
    if(first_time) {
        for(i = 0; i < 6; i ++) {
            h->local_histograms[i].count = NULL;
            h->local_histograms[i].bins.lower_bounds = NULL;
            h->global_histograms[i].count = NULL;
            h->global_histograms[i].bins.lower_bounds = NULL;
        }
    }

    // set up bin boundaries
    // X
    rc = initialize_hostogram(&h->local_histograms[0],
                              0,
                              gts_global_features->mi_gmin_x,
                              gts_global_features->mi_gmax_x,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram(&h->global_histograms[0],
                                  0,
                                  gts_global_features->mi_gmin_x,
                                  gts_global_features->mi_gmax_x,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }    

    // Y
    rc = initialize_hostogram(&h->local_histograms[1],
                              1,
                              gts_global_features->mi_gmin_y,
                              gts_global_features->mi_gmax_y,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram(&h->global_histograms[1],
                                  1,
                                  gts_global_features->mi_gmin_y,
                                  gts_global_features->mi_gmax_y,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    // Z
    rc = initialize_hostogram(&h->local_histograms[2],
                              2,
                              gts_global_features->mi_gmin_z,
                              gts_global_features->mi_gmax_z,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram(&h->global_histograms[2],
                                  2,
                                  gts_global_features->mi_gmin_z,
                                  gts_global_features->mi_gmax_z,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    // V1
    rc = initialize_hostogram(&h->local_histograms[3],
                              3,
                              gts_global_features->mi_gmin_v1,
                              gts_global_features->mi_gmax_v1,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }    
    if(rank == 0) {
        rc = initialize_hostogram(&h->global_histograms[3],
                                  3,
                                  gts_global_features->mi_gmin_v1,
                                  gts_global_features->mi_gmax_v1,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    // W
    rc = initialize_hostogram(&h->local_histograms[4],
                              4,
                              gts_global_features->mi_gmin_w,
                              gts_global_features->mi_gmax_w,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram(&h->global_histograms[4],
                                  4,
                                  gts_global_features->mi_gmin_w,
                                  gts_global_features->mi_gmax_w,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    // V2
    rc = initialize_hostogram(&h->local_histograms[5],
                              5,
                              gts_global_features->mi_gmin_v2,
                              gts_global_features->mi_gmax_v2,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram(&h->global_histograms[5],
                                  5,
                                  gts_global_features->mi_gmin_v2,
                                  gts_global_features->mi_gmax_v2,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }
    return 0;
}
    
/*
 * histogram local calculation and global reduction
 */ 
int histogram_analysis_loop(histogram_data *h,
                            struct timestep_data * pgs,
                            int num_pgs, 
                            int is_zion,                            
                            int istep,
                            int rank,
                            MPI_Comm comm
                           )
{    
    int i, j, rc;
    double *zion;
    int mi;
    for(j = 0; j < num_pgs; j ++) {        
//fprintf(stderr, "j=%d %s:%d\n", j,__FILE__,__LINE__);
        if(is_zion) {    
            zion = pgs [j].zion;
            mi = pgs [j].mi;
        }
        else {
            zion = pgs [j].zeon;
            mi = pgs [j].me;
        }
        
        // calculate local histogram
        for(i = 0; i < mi; i +=7) {
            update_count(zion[i], &h->local_histograms[0]);
            update_count(zion[i+1], &h->local_histograms[1]);
            update_count(zion[i+2], &h->local_histograms[2]);
            update_count(zion[i+3], &h->local_histograms[3]);
            update_count(zion[i+4], &h->local_histograms[4]);
            update_count(zion[i+5], &h->local_histograms[5]);
        }
    }

    // aggregate to rank 0 for global histogram
    for(i = 0; i < 6; i ++) {
        rc = MPI_Reduce(h->local_histograms[i].count,
                h->global_histograms[i].count,
                h->local_histograms[i].num_bins,
                MPI_LONG,
                MPI_SUM,
                0,
                comm);
    }

    if(rank == 0) {
        char file_name[100];
        if(is_zion) {
            sprintf(file_name, "zion_");
        }
        else {
            sprintf(file_name, "zeon_");
        }
        FILE *f;
        histogram *hist;
        for(i = 0; i < 6; i ++) {
            sprintf(file_name+5, "histogram_%d.%d\0", i, istep);
            f = fopen(file_name, "w");
            if(!f) {
                fprintf(stderr, "cannot open file (%s)\n", file_name);
                return -1;
            }
            hist = &h->global_histograms[i];
            int j;
            for(j =0; j < hist->num_bins; j ++) {
                fprintf(f, "%d\t%10f\t%lu\n", j, hist->bins.lower_bounds[j], hist->count[j]);
                //fprintf(histogram_file, "%d\t%lu\n", j, histograms[i].count[j]);
            }
            fclose(f);
        }
    }

    return 0;
}

/////////////////////////////////////////////////////////////
//             End of Histogram                            //
/////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////
//             2D Histogram                                //
/////////////////////////////////////////////////////////////


#define NUM_BINS 1000

typedef struct _histogram_2d_data
{
    bin_setting bins[6];
    histogram_2d local_histograms[5];
    histogram_2d global_histograms[5];
} histogram_2d_data;

histogram_2d_data histograms_2d;
histogram_2d_data histograms_2d_e;

/*
 * 2d histogram per-timestep initialization
 */
int histogram_2d_analysis_init(gts_local *gts_local_features,
                               gts_global *gts_global_features,
                               histogram_2d_data *h,
                               int rank,
                               int first_time
                              )
{
    int i, rc;

    // TODO: move to initialization
    if(first_time) {
        for(i = 0; i < 6; i ++) {
            h->bins[i].lower_bounds = NULL;
        }
        for(i = 0; i < 5; i ++) {
            h->local_histograms[i].count = NULL;
            h->local_histograms[i].a1_bins = &(h->bins[i]);
            h->local_histograms[i].a2_bins = &(h->bins[i+1]);
            h->global_histograms[i].count = NULL;
            h->global_histograms[i].a1_bins = &(h->bins[i]);
            h->global_histograms[i].a2_bins = &(h->bins[i+1]);
        }
    }

    // set up bin boundaries
    // X
    setup_uniform_binning(&(h->bins[0]),
                          0,
                          gts_global_features->mi_gmin_x,
                          gts_global_features->mi_gmax_x,
                          NUM_BINS
                         );

    // Y
    setup_uniform_binning(&(h->bins[1]),
                          1,
                          gts_global_features->mi_gmin_y,
                          gts_global_features->mi_gmax_y,
                          NUM_BINS
                         );

    // Z
    setup_uniform_binning(&(h->bins[2]),
                          2,
                          gts_global_features->mi_gmin_z,
                          gts_global_features->mi_gmax_z,
                          NUM_BINS
                         );

    // V1
    setup_uniform_binning(&(h->bins[3]),
                          3,
                          gts_global_features->mi_gmin_v1,
                          gts_global_features->mi_gmax_v1,
                          NUM_BINS
                         );

    // W
    setup_uniform_binning(&(h->bins[4]),
                          4,
                          gts_global_features->mi_gmin_w,
                          gts_global_features->mi_gmax_w,
                          NUM_BINS
                         );

    // V2
    setup_uniform_binning(&(h->bins[5]),
                          5,
                          gts_global_features->mi_gmin_v2,
                          gts_global_features->mi_gmax_v2,
                          NUM_BINS
                         );


    // set up local and global 2d histogram

    // X and Y
    rc = initialize_hostogram_2d(&h->local_histograms[0],
                              0,
                              1,
                              NUM_BINS,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram_2d(&h->global_histograms[0],
                                  0,
                                  1,
                                  NUM_BINS,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    // Y and Z
    rc = initialize_hostogram_2d(&h->local_histograms[1],
                              1,
                              2,
                              NUM_BINS,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram_2d(&h->global_histograms[1],
                                  1,
                                  2,
                                  NUM_BINS,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    // Z and V1
    rc = initialize_hostogram_2d(&h->local_histograms[2],
                              2,
                              3,
                              NUM_BINS,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram_2d(&h->global_histograms[2],
                                  2,
                                  3,
                                  NUM_BINS,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }
    // V1 and W
    rc = initialize_hostogram_2d(&h->local_histograms[3],
                              3,
                              4,
                              NUM_BINS,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram_2d(&h->global_histograms[3],
                                  3,
                                  4,
                                  NUM_BINS,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    // W and V2
    rc = initialize_hostogram_2d(&h->local_histograms[4],
                              4,
                              5,
                              NUM_BINS,
                              NUM_BINS
                             );
    if(rc) {
        return rc;
    }
    if(rank == 0) {
        rc = initialize_hostogram_2d(&h->global_histograms[4],
                                  4,
                                  5,
                                  NUM_BINS,
                                  NUM_BINS
                                 );
        if(rc) {
            return rc;
        }
    }

    return 0;
}

int histogram_2d_analysis_loop(histogram_2d_data *h,
                            int is_zion,
                            int istep,
                            int rank,
                            MPI_Comm comm
                           )
{

    int i, j, k, rc;
    double *zion;
    int mi;

    if(is_zion) {
        zion = qu_mi;
        mi = my_mi;
    }
    else {
        zion = qu_me;
        mi = my_me;
    }

    // calculate local histogram
    for(i = 0; i < mi; i +=7) {
        update_count_2d(zion[i], zion[i+1], &h->local_histograms[0]);
        update_count_2d(zion[i+1], zion[i+2], &h->local_histograms[1]);
        update_count_2d(zion[i+2], zion[i+3], &h->local_histograms[2]);
        update_count_2d(zion[i+3], zion[i+4], &h->local_histograms[3]);
        update_count_2d(zion[i+4], zion[i+5], &h->local_histograms[4]);
    }

    // aggregate to rank 0 for global histogram
    for(i = 0; i < 5; i ++) {
        rc = MPI_Reduce(h->local_histograms[i].count,
                h->global_histograms[i].count,
                h->local_histograms[i].a1_num_bins * h->local_histograms[i].a2_num_bins,
                MPI_LONG,
                MPI_SUM,
                0,
                comm);
    }

    if(rank == 0) {
        char file_name[100];
        if(is_zion) {
            sprintf(file_name, "zion_");
        }
        else {
            sprintf(file_name, "zeon_");
        }
        FILE *f;
        histogram_2d *hist;
        for(i = 0; i < 5; i ++) {
            sprintf(file_name+5, "histogram_2d_%d.%d\0", i, istep);
            f = fopen(file_name, "w");
            if(!f) {
                fprintf(stderr, "cannot open file (%s)\n", file_name);
                return -1;
            }
            hist = &h->global_histograms[i];

            uint64_t size_to_write, size_written;
            size_to_write = hist->a1_num_bins * hist->a2_num_bins;

            // TODO: writing text takes ~12 seconds, so don't do it if time is limited
            size_written = fwrite(hist->count, sizeof(uint64_t), size_to_write, f);

            if(size_written != size_to_write) {
                fprintf(stderr, "only write %lu of %lu to file %s. %s:%d\n",
                    size_written, size_to_write, file_name, __FILE__, __LINE__);
            }
            fclose(f);
        }
    }
    return 0;
}
/////////////////////////////////////////////////////////////
//             End of 2D Histogram                         //
/////////////////////////////////////////////////////////////

// for timing
long long restart_read_prof;
long long fopen_prof, fclose_prof;
long long local_min_prof;
long long global_min_prof;
long long range_query_prof;
long long histogram_prof;
long long histogram_2d_prof;

/*
 * main body of analysis. run as a MPI program and co-locate 
 * with GTS simulation
 */
int main (int argc, char *argv[])
{

    // local variables
    int numtasks, rank, rc, num_mi, rt;
    int i;

    gts_local gl_features;
    gts_global gg_features;
    gts_local gl_features_e;
    gts_global gg_features_e;
    gts_local  *gts_local_features;
    gts_global *gts_global_features;
    gts_local  *gts_local_features_e;
    gts_global *gts_global_features_e;

    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Init (&argc, &argv);    
//    int provided;
//    rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE,&provided);
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);    
    MPI_Comm_size (MPI_COMM_WORLD, &numtasks); 
//    MPI_Query_thread(&provided);

printf ("top of the main\n");
    // pin main thread
/*
    node_topology_t node_topo = get_node_topology();

    int num_nodes;
    get_num_nodes(&num_nodes);
    if(num_nodes == -1) {
        // assume one process per node
        num_nodes = numtasks;
    }
    int byslot = 1;

    // TODO: have some rule to determine the core id according to
    // node topology, mpi rank, number of reader processes per node, etc.
    // here we use a simple rule: sequentially assign to each numa domain
    int num_readers_per_node = numtasks / num_nodes;
    int id_on_node;
    if(byslot) {
        id_on_node = rank % num_readers_per_node;
    }
    else {
        id_on_node = rank / num_readers_per_node;
    }
    // first core in each numa domain
    int core_id = id_on_node * (node_topo->num_cores/node_topo->num_numa_domains); 
    pin_thread_self_to_core(core_id);
    print_thread_self_affinity(rank, 0);
*/
    // pin thread
    int num_nodes;
    char *temp_string = getenv("NUM_STAGING_NODES");
    if(temp_string) {
        num_nodes = atoi(temp_string);
    }
    else {
        fprintf(stderr, "environment variable NUM_STAGING_NODES is not set.\n");
        num_nodes = 64;
    }
    int byslot = 1;

    // TODO: have some rule to determine the core id according to
    // node topology, mpi rank, number of reader processes per node, etc.
    // here we use a simple rule: sequentially assign to each numa domain
    int num_readers_per_node = numtasks / num_nodes;
    int id_on_node;
    if(byslot) {
        id_on_node = rank % num_readers_per_node;
    }
    else {
        id_on_node = rank / num_readers_per_node;
    }
    int core_id = id_on_node * 2 + 1;

printf ("top of the main 2\n");
    //cpu_set_t cpuset;
    //CPU_ZERO(&cpuset);
    //CPU_SET(core_id, &cpuset);
    //rc = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    //if(rc) {
    //    fprintf(stderr, "cannot set application thread affinity\n");
    //    perror("sched_setaffinity");
    //}
//fprintf(stderr, "staging main thread %d on core %d\n", rank, core_id);


    // TODO: pass in parameters
    char *fname = "RESTART.bp";

    // for timing
    init_prof_simulation("staging.prof",
                         30, // number of cycles 
                         10, // number of profiling points
                         comm
                        );
    init_prof_read_all();

    simulation_start();

    // phase 1: initialization
    
printf ("top of the main 3\n");
    MPI_Barrier(comm);

/////////////////////////////////////////////////////////////////////////
#if XPMEM_READ
    enum ADIOS_READ_METHOD method = ADIOS_READ_METHOD_XPMEM;
    char * method_parameters = "";
    enum ADIOS_LOCKMODE lock_mode = ADIOS_LOCKMODE_ALL;
#else
    enum ADIOS_READ_METHOD method = ADIOS_READ_METHOD_BP;
    char * method_parameters = "verbose=3";
    enum ADIOS_LOCKMODE lock_mode = ADIOS_LOCKMODE_NONE;
#endif
    float timeout_sec = 0;
    int from_timestep = 0;
    int num_timesteps = 1;
    int blocking_read = true;

    ADIOS_SELECTION * selection = NULL;
/////////////////////////////////////////////////////////////////////////

printf ("top of the main 4\n");
    adios_read_init_start();

    //set_application_id(2);
    //rc = adios_read_ib_init(comm);
    rc = adios_read_init_method (method, comm, method_parameters);
printf ("top of the main 5\n");

    adios_read_init_end();
printf ("top of the main 5a\n");

    // initialization of analysis
    gts_local_features = &gl_features;
    gts_global_features = &gg_features;
    gts_local_features_e = &gl_features_e;
    gts_global_features_e = &gg_features_e;

    // phase 2: read data from file and do analysis
    int time_index = 0;
    
    // open file
    MPI_Barrier(comm);

    // for timing
    prof_point_start(&fopen_prof, 1, 0);
printf ("top of the main 6\n");
    
    //ib_read_file_data *f = adios_read_ib_fopen(fname, comm, 1);
//ADIOS_FILE * f = adios_read_open ("./RESTART/DATA_RESTART.00000", method, comm, lock_mode, timeout_sec);
// you have to use adios_read_open_file or it doesn't work even though there
// are no generated errors except a seg fault.
#if XPMEM_READ
printf ("start xpmem_read\n");
 //ADIOS_FILE *f = adios_read_open_stream ("./RESTART/DATA_RESTART.00000", method, comm, lock_mode, 2);
 ADIOS_FILE *f = adios_read_open_file("./RESTART/DATA_RESTART.00000", method, comm);
printf ("end xpmem_read\n");
#else
 ADIOS_FILE *f = adios_read_open_file("./RESTART/DATA_RESTART.00000", method, comm);
assert (f);
#endif
    // always just read from a particular rank
#if XPMEM_READ
#else
//    selection = adios_selection_writeblock (rank);
//assert (selection);
#endif

    prof_point_end(fopen_prof, 1, 0);

    if(!f) {
        fprintf(stderr, "cannot open file. %s:%d\n", __FILE__, __LINE__);
        return;
    }

    simulation_main_loop_start();   

    // keep reading new timesteps
    rc = 0;
    while(rc == 0) {
#if DEBUG_OUTPUT
printf ("start of loop 0 rc: %d (%d)\n", rc, err_end_of_stream);
#endif

        cycle_start(time_index);
#if DEBUG_OUTPUT
printf ("start of loop 1\n");
#endif

        // poll for next available timestep

        fopen_start(&restart_read_prof, fname, time_index, comm);
#if DEBUG_OUTPUT
printf ("start of loop 2\n");
#endif
        
        //rc = adios_read_ib_get_timestep(f, time_index);       
        //timestep is implicit based on read API for this version
        //still need some way to detect exit

        fopen_end(restart_read_prof, (long long)f, time_index);
#if DEBUG_OUTPUT
printf ("start of loop 3\n");
#endif

        if(rc == 1) {

            cycle_end(time_index);

            printf("Reached EOF. exit.\n");
            break;
        }
#if DEBUG_OUTPUT
printf ("start of loop 4\n");
#endif

        // initialization before processing each timestep
        memset(gts_local_features, 0, sizeof(gts_local));
#if DEBUG_OUTPUT
printf ("start of loop 5\n");
#endif
        memset(gts_global_features, 0, sizeof(gts_global));
#if DEBUG_OUTPUT
printf ("start of loop 6\n");
#endif
        memset(gts_local_features_e, 0, sizeof(gts_local));
#if DEBUG_OUTPUT
printf ("start of loop 7\n");
#endif
        memset(gts_global_features_e, 0, sizeof(gts_global));
#if DEBUG_OUTPUT
printf ("start of loop 8\n");
#endif

        //Read through all of the PGs to generate global stats
        //then re-read each one and process for the histograms and other stuff
        //change this to save these blobs for later use.
        //pg_info_ib_t pgs = f->timestep->pgs;
        int i;
        int first_pg = 1;
#define ZION_ID 10
        //ADIOS_VARINFO * info = adios_inq_var (f, "zion");
#if DEBUG_OUTPUT
printf ("start of loop 9\n");
#endif
        int num_pgs = 1; // this is to be hard-coded for now according to
                         // Hasan Abbasi (ORN)
                         // info->nblocks [0]; is the potential alternative
        //adios_free_varinfo (info);
#if DEBUG_OUTPUT
printf ("start of loop 10\n");
#endif
        struct timestep_data * pgs = (struct timestep_data *) malloc (sizeof (struct timestep_data) * num_pgs);
#if DEBUG_OUTPUT
printf ("start of loop 11\n");
#endif
        for(i = 0; i < num_pgs; i++) {
#if DEBUG_OUTPUT
printf ("(%d) i: %d num_pgs: %d\n", rank, i, num_pgs);
#endif

            // per pg processing

            inq_var_start(restart_read_prof, time_index);

            // get reference of variables within pg
            int64_t istep;
            int32_t mype;
            int32_t numberpe;
            int32_t nparam;
            int32_t mi;
            int32_t mi_total;
            int32_t mi_offset;
            int32_t ntracer;
            int32_t ntracer_i0;
            int32_t misum;
            double * zion;
            int32_t ntracer_e;
            int32_t etracer_e0;
            int32_t mesum;
            int32_t me;
            int32_t me_total;
            int32_t me_offset;
            double * zeon;

#if XPMEM_READ
            selection = adios_selection_boundingbox (0, 0, 0);
            adios_schedule_read (f, selection, "nparam", from_timestep, num_timesteps, &nparam);
            adios_schedule_read (f, selection, "mi_total", from_timestep, num_timesteps, &mi_total);
            adios_schedule_read (f, selection, "me_total", from_timestep, num_timesteps, &me_total);
            rc = adios_perform_reads (f, blocking_read);
assert (!rc);
            adios_selection_delete (selection);
            selection = NULL;
#else
            selection = adios_selection_writeblock (rank);
            adios_schedule_read (f, selection, "nparam", from_timestep, num_timesteps, &nparam);
            adios_schedule_read (f, selection, "mi_total", from_timestep, num_timesteps, &mi_total);
            adios_schedule_read (f, selection, "me_total", from_timestep, num_timesteps, &me_total);
            rc = adios_perform_reads (f, blocking_read);
assert (!rc);
            adios_selection_delete (selection);
            selection = NULL;
#endif

#if DEBUG_OUTPUT
printf ("(%d) mi_total: %d me_total: %d nparam: %d\n", rank, mi_total, me_total, nparam);
#endif
            zion = (double *) malloc (mi_total * nparam * sizeof (double));
            zeon = (double *) malloc (me_total * nparam * sizeof (double));

#if XPMEM_READ
            selection = adios_selection_boundingbox (0, 0, 0);
#else
            selection = adios_selection_writeblock (rank);
#endif
            adios_schedule_read (f, selection, "istep", from_timestep, num_timesteps, &istep);
            adios_schedule_read (f, selection, "mype", from_timestep, num_timesteps, &mype);
            adios_schedule_read (f, selection, "numberpe", from_timestep, num_timesteps, &numberpe);
            adios_schedule_read (f, selection, "mi", from_timestep, num_timesteps, &mi);
            adios_schedule_read (f, selection, "mi_offset", from_timestep, num_timesteps, &mi_offset);
            adios_schedule_read (f, selection, "ntracer", from_timestep, num_timesteps, &ntracer);
            adios_schedule_read (f, selection, "ntracer_i0", from_timestep, num_timesteps, &ntracer_i0);
            adios_schedule_read (f, selection, "misum", from_timestep, num_timesteps, &misum);
            adios_schedule_read (f, selection, "ntracer_e", from_timestep, num_timesteps, &ntracer_e);
            adios_schedule_read (f, selection, "etracer_e0", from_timestep, num_timesteps, &etracer_e0);
            adios_schedule_read (f, selection, "mesum", from_timestep, num_timesteps, &mesum);
            adios_schedule_read (f, selection, "me", from_timestep, num_timesteps, &me);
            adios_schedule_read (f, selection, "me_offset", from_timestep, num_timesteps, &me_offset);

           rc = adios_perform_reads (f, blocking_read);
assert (!rc);

           adios_selection_delete (selection);
           selection = NULL;

#if XPMEM_READ
            uint64_t start [2] = {0, 0};
            uint64_t count [2];
            count [0] = nparam;
            count [1] = mi_total;
            selection = adios_selection_boundingbox (1, start, count);
//            selection = adios_selection_boundingbox (1, start, count);
            selection = adios_selection_writeblock (rank);
assert (selection);
#else
            selection = adios_selection_writeblock (rank);
#endif

            adios_schedule_read (f, selection, "zion", from_timestep, num_timesteps, zion);
//            adios_schedule_read (f, selection, "zeon", from_timestep, num_timesteps, zeon);

#if DEBUG_OUTPUT
printf ("(%d) 11\n", rank);
#endif
            rc = adios_perform_reads (f, blocking_read);
assert (!rc);
assert (zion);
assert (zeon);
#if DEBUG_OUTPUT
printf ("(%d) 12 i: %d\n", rank, i);
#endif
            adios_selection_delete (selection);
            selection = NULL;

            pgs [i].mi = mi;
            pgs [i].zion = zion;
            pgs [i].me = me;
            pgs [i].zeon = zeon;

            inq_var_end(restart_read_prof, time_index);

            prof_point_start(&local_min_prof, 11, time_index);
     
            // local analysis and aggregation
assert (gts_local_features);
assert (gts_local_features_e);
            rt = Analyp_combine(zion, mi, gts_local_features, first_pg);

            // local analysis and aggregation
            rt = Analyp_combine(zeon, me, gts_local_features_e, first_pg);

            first_pg = 0;

#if DEBUG_OUTPUT
printf ("(%d) 13\n", rank);
#endif
            prof_point_end(local_min_prof, 11, time_index);
#if DEBUG_OUTPUT
printf ("(%d) 14\n", rank);
#endif
        }

#if DEBUG_OUTPUT
printf ("(%d) 15\n", rank);
#endif
        // cross pg processing

        prof_point_start(&global_min_prof, 12, time_index);
#if DEBUG_OUTPUT
printf ("(%d) 16\n", rank);
#endif

        // global aggregation among analysis processes
        global_minmax(gts_local_features,  
                      gts_global_features,
                      comm
                     );
#if DEBUG_OUTPUT
printf ("(%d) 17\n", rank);
#endif

        global_minmax(gts_local_features_e,
                      gts_global_features_e,
                      comm
                     );

#if DEBUG_OUTPUT
printf ("(%d) 18\n", rank);
#endif
        prof_point_end(global_min_prof, 12, time_index);


/////////////////////////////////////////////////////////////
//             Histogram                                   //
/////////////////////////////////////////////////////////////

#if DEBUG_OUTPUT
printf ("(%d) 19\n", rank);
#endif
        prof_point_start(&histogram_prof, 15, time_index);
        
        // zion
        rc = histogram_analysis_init(gts_local_features, 
                                     gts_global_features,
                                     &histograms,
                                     rank,
                                     (time_index==0)?1:0
                                    );

#if DEBUG_OUTPUT
printf ("(%d) 20\n", rank);
#endif
        rc = histogram_analysis_loop(&histograms,
                                     pgs,
                                     num_pgs, 
                                     1, // zion                                     
                                     time_index*2, // every 2 simulation timestep
                                     rank,
                                     comm
                                    );
#if DEBUG_OUTPUT
printf ("(%d) 21\n", rank);
#endif
        // zeon
        rc = histogram_analysis_init(gts_local_features_e, 
                                     gts_global_features_e,
                                     &histograms_e,
                                     rank,
                                     (time_index==0)?1:0
                                    );

#if DEBUG_OUTPUT
printf ("(%d) 22\n", rank);
#endif
#if HAVE_ELECTRONS
        rc = histogram_analysis_loop(&histograms_e,
                                     pgs,
                                     num_pgs, 
                                     0, // zeon                                     
                                     time_index*2, // every 2 simulation timestep
                                     rank,
                                     comm
                                    );
#endif

#if DEBUG_OUTPUT
printf ("(%d) 23\n", rank);
#endif
        prof_point_end(histogram_prof, 15, time_index);

/////////////////////////////////////////////////////////////
//             End of Histogram                            //
/////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////
//                   Range Qurery                          //
/////////////////////////////////////////////////////////////

        prof_point_start(&range_query_prof, 13, time_index);

        // zion
        rc = range_query_analysis(gts_global_features,
                                  pgs,
                                  num_pgs, 
                                  1,                            
                                  rank,
                                  comm
                                 );

#if HAVE_ELECTRONS
        // zeon
        rc = range_query_analysis(gts_global_features_e,
                                  pgs,
                                  num_pgs, 
                                  0,                            
                                  rank,
                                  comm
                                 );
#endif

        prof_point_end(range_query_prof, 13, time_index);

/////////////////////////////////////////////////////////////
//             End of Range Qurery                         //
/////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////
//             2D Histogram                                //
/////////////////////////////////////////////////////////////

        prof_point_start(&histogram_2d_prof, 14, time_index);

        // zion
        // local analysis and aggregation
        Analyp(qu_mi, my_mi, gts_local_features);

        // global aggregation among analysis processes
        global_minmax(gts_local_features,
                      gts_global_features,
                      comm
                     );

        rc = histogram_2d_analysis_init(gts_local_features,
                                     gts_global_features,
                                     &histograms_2d,
                                     rank,
                                     (time_index==0)?1:0
                                    );

        rc = histogram_2d_analysis_loop(&histograms_2d,
                                     1, // zion
                                     time_index*2, // every 2 simulation timestep
                                     rank,
                                     comm
                                    );


#if HAVE_ELECTRONS
        // zeon
        // local analysis and aggregation
        Analyp(qu_me, my_me, gts_local_features_e);

        global_minmax(gts_local_features_e,
                      gts_global_features_e,
                      comm
                     );

        rc = histogram_2d_analysis_init(gts_local_features_e,
                                     gts_global_features_e,
                                     &histograms_2d_e,
                                     rank,
                                     (time_index==0)?1:0
                                    );

        rc = histogram_2d_analysis_loop(&histograms_2d_e,
                                     0, // zeon
                                     time_index*2, // every 2 simulation timestep
                                     rank,
                                     comm
                                    );
#endif

        prof_point_end(histogram_2d_prof, 14, time_index);
/////////////////////////////////////////////////////////////
//             End of 2D Histogram                         //
/////////////////////////////////////////////////////////////

        gclose_start(restart_read_prof, time_index);

        // release pgs of current timestep
#if USE_EVPATH
        for(i = 0; i < f->timestep->num_pgs; i ++) {
            adios_read_ib_release_pg(&pgs[i]);
        }
#endif
#if DEBUG_OUTPUT
printf ("cleanup pgs here 1\n");
#endif

        gclose_end(restart_read_prof, time_index);
#if DEBUG_OUTPUT
printf ("cleanup pgs here 2\n");
#endif

        fclose_start(restart_read_prof, time_index);
#if DEBUG_OUTPUT
printf ("cleanup pgs here 3\n");
#endif

        // proceed to the next timestep
#if USE_EVPATH
        adios_read_ib_advance_timestep(f);
#endif
#if DEBUG_OUTPUT
printf ("cleanup pgs here 4\n");
#endif

        adios_release_step (f);
#if DEBUG_OUTPUT
printf ("(%d) release step\n", rank);
#endif
        rc = adios_advance_step (f, 0, 1);
#if DEBUG_OUTPUT
printf ("advance step rc = %d\n", rc);
#endif

        fclose_end(restart_read_prof, time_index);
#if DEBUG_OUTPUT
printf ("cleanup pgs here 5\n");
#endif

        cycle_end(time_index);
#if DEBUG_OUTPUT
printf ("cleanup pgs here 6\n");
#endif

        time_index ++;
#if DEBUG_OUTPUT
printf ("end of loop rc: %d\n", rc);
#endif
    }
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 0\n", rank);
#endif

    simulation_main_loop_end();
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 1\n", rank);
#endif

    // finalize before closing file

    // close
    prof_point_start(&fclose_prof, 2, 0);
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 2\n", rank);
#endif

    //adios_read_ib_fclose(f);

    prof_point_end(fclose_prof, 2, 0);
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 3\n", rank);
#endif

    // phase 3: finalize
    MPI_Barrier(comm);
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 4\n", rank);
#endif

    adios_read_finalize_start();

#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 5\n", rank);
#endif
    //adios_read_ib_finalize();

    adios_read_finalize_end();
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 6\n", rank);
#endif

    simulation_end();
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 7\n", rank);
#endif
    finalize_prof_read_all();   
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 8\n", rank);
#endif
    finalize_prof_simulation();
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 9\n", rank);
#endif

    MPI_Finalize();
#if DEBUG_OUTPUT
printf ("(%d) exiting analysis loop 10\n", rank);
#endif
    return 0;
}

