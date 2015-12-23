/* 
 * This program performs analysis (mainly range query, histogram, and transpose)
 * on GTS particle output data. It runs as a MPI program on dedicated cores in
 * compute nodes where GTS simulation runs. Particle data is transferred through
 * shm transport.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "shm_mm.h"
#include "shm_malloc.h"
#include "shm_queue.h"
#include "meta_data.h"
#include "read_shm.h"

#include <math.h>
#include "Analyp.h"
#include "histogram.h"

// for timing
#include "adios_read_timing.h"
#include "adios_sim_timing.h"

// for thread pinning
#include "thread_pin.h"
#include <sched.h>

// global variable to carry state cross timesteps
int my_mi;
double *qu_mi = NULL;
int my_me;
double *qu_me = NULL;

double *transpose_buf;
int first_time_transpose = 1;

/////////////////////////////////////////////////////////////
//                   Range Qurery                          //
/////////////////////////////////////////////////////////////
int range_query_analysis(gts_global *gts_global_features,
                         shm_pg_info_t pgs,
                         int num_pgs, 
                         int is_zion,                            
                         int rank,
                         MPI_Comm comm
                        )
{

    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);
    double t1 = timestamp.tv_sec + timestamp.tv_usec / 1.0e6;

    int numtasks;
    MPI_Comm_size(comm, &numtasks);

    double *rbuf;
    int *counts, *displs;
    int sum = 0;

    long int num_mi = 0; // TODO: reset to 0 every timestep
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
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__,__LINE__);
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

    gettimeofday(&timestamp, NULL);
    double t2 = timestamp.tv_sec + timestamp.tv_usec / 1.0e6;

    int i;
    shm_var_info *var;
    double *zion;
    int *mi;
    for(i = 0; i < num_pgs; i ++) {        
        // TODO: only process the pg mapped to this reader
        if(rank%4 != i) continue;

        if(is_zion) {    
            var = adios_read_shm_get_var_byname(&pgs[i], "zion");
            zion = (double *)var->data;
            var = adios_read_shm_get_var_byname(&pgs[i], "mi");
            mi = (int *)var->data;
        }
        else {
            var = adios_read_shm_get_var_byname(&pgs[i], "zeon");
            zion = (double *)var->data;
            var = adios_read_shm_get_var_byname(&pgs[i], "me");
            mi = (int *)var->data;
        }

        // filter local array and pack filtered particles to qu_mi
        Scanfilter_combine(zion, *mi, *qu, &num_mi, Lolimit, Uplimit);

fprintf(stderr, "rank=%d pgi=%d total=%d result=%d\n",rank,i,*mi,num_mi);
    }

    if(is_zion) {
        my_mi = num_mi;
    }
    else {
        my_me = num_mi;
    }

    gettimeofday(&timestamp, NULL);
    double t3 = timestamp.tv_sec + timestamp.tv_usec / 1.0e6;

#if PRINT_RESULT
        for(i = 0; i < num_mi*7; i++){
            if((i % 7) == 0) printf("\n");
            printf("%lf ", (*qu)[i]);
        }
        printf("\n");
        printf ("Number of tasks= %d My rank= %d\n", numtasks,rank);
        printf("--------------------------------------\n");
#endif

fprintf(stderr, "rank %d t1 %f t2 %f t3 %f \n", rank, t1, t2, t3);

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
    MPI_Allreduce(&(gts_local_features->mi_cnt), &(gts_global_features->mi_gcnt), 1, MPI_INT, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_min_x), &(gts_global_features->mi_gmin_x), 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(&(gts_local_features->mi_max_x), &(gts_global_features->mi_gmax_x), 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(&(gts_local_features->mi_sqr_sum_x), &(gts_global_features->mi_sqr_sum_x), 1, MPI_DOUBLE, MPI_SUM, comm);
    MPI_Allreduce(&(gts_local_features->mi_sum_x), &(gts_global_features->mi_sum_sqr_x), 1, MPI_DOUBLE, MPI_SUM, comm);


#if PRINT_RESULT
    printf("\nHong Fang X : %d , %d , %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
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
    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
                            shm_pg_info_t pgs,
                            int num_pgs, 
                            int is_zion,                            
                            int istep,
                            int rank,
                            MPI_Comm comm
                           )
{    
    int i, j, rc;
    shm_var_info *var;
    double *zion;
    int *mi;
    for(j = 0; j < num_pgs; j ++) {        
        // TODO: only process the pg mapped to this reader
        if(rank%4 != j) continue;

        if(is_zion) {    
            var = adios_read_shm_get_var_byname(&pgs[j], "zion");
            zion = (double *)var->data;
            var = adios_read_shm_get_var_byname(&pgs[j], "mi");
            mi = (int *)var->data;
        }
        else {
            var = adios_read_shm_get_var_byname(&pgs[j], "zeon");
            zion = (double *)var->data;
            var = adios_read_shm_get_var_byname(&pgs[j], "me");
            mi = (int *)var->data;
        }
        
        // calculate local histogram
        for(i = 0; i < *mi; i +=7) {
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
                9,
                comm);
    }

    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
    if(rank == 9) {
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
                9,
                comm);
    }

    if(rank == 9) {
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
//    MPI_Comm comm;
//    MPI_Comm_dup(MPI_COMM_WORLD, &comm);
    MPI_Init (&argc, &argv);    
    MPI_Comm_rank (MPI_COMM_WORLD, &rank);    
    MPI_Comm_size (MPI_COMM_WORLD, &numtasks); 

    printf("Hello world from process %d of %d\n", rank, numtasks);

    // pin main thread
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

    if(id_on_node == 3) {
        id_on_node = 2;
    }
    else if(id_on_node == 2) {
        id_on_node = 3;
    }

    // first core in each numa domain
    int core_id = id_on_node * (node_topo->num_cores/node_topo->num_numa_domains) + 3; 

//    core_id = (core_id+4)%16;

    pin_thread_self_to_core(core_id);

#if 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    CPU_SET(core_id-1, &cpuset);
    CPU_SET(core_id-2, &cpuset);
    rc = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    if(rc) {
        fprintf(stderr, "cannot set application thread affinity\n");
        perror("sched_setaffinity");
    }
#endif
    print_thread_self_affinity(rank, 0);

    // TODO: pass in parameters
    char *fname = "RESTART.bp";

    // for timing
    init_prof_simulation("helper.prof",
                         30, // number of cycles 
                         10, // number of profiling points
                         comm
                        );
    init_prof_read_all();

    simulation_start();

    // phase 1: initialization
    
    MPI_Barrier(comm);

    adios_read_init_start();

    rc = adios_read_shm_init(comm);

    adios_read_init_end();

    // initialize ADIOS write API
//    adios_init("rangequery.xml");
  
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
    
    shm_read_file_data *f = adios_read_shm_fopen(fname, comm, 1);

    prof_point_end(fopen_prof, 1, 0);

    if(!f) {
        fprintf(stderr, "cannot open file. %s:%d\n", __FILE__, __LINE__);
        return;
    }

    simulation_main_loop_start();   

    // keep reading new timesteps
    while(1) {

        cycle_start(time_index);

        // poll for next available timestep

        fopen_start(&restart_read_prof, fname, time_index, comm);
        
        rc = adios_read_shm_get_timestep(f, time_index);       

        fopen_end(restart_read_prof, (long long)f, time_index);

        if(rc == 1) {

            cycle_end(time_index);

            printf("Reached EOF. exit.\n");
            break;
        }

        // initialization before processing each timestep
        memset(gts_local_features, 0, sizeof(gts_local));
        memset(gts_global_features, 0, sizeof(gts_global));
        memset(gts_local_features_e, 0, sizeof(gts_local));
        memset(gts_global_features_e, 0, sizeof(gts_global));


        shm_pg_info_t pgs = f->pgs;
        int i;
        int first_pg = 1;
        for(i = 0; i < f->num_pgs; i ++) {

            // per pg processing
//            fprintf(stderr, "reader rank=%d pgrank=%d i=%d\n",rank, pgs[i].data_q->writer_rank, i);
            // TODO: only process the pg mapped to this reader
            if(rank%4 != i) continue;

            inq_var_start(restart_read_prof, time_index);

            // get reference of variables within pg
            shm_var_info *var;

            var = adios_read_shm_get_var_byname(&pgs[i], "istep");
            int *istep = (int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "mype");
            int *mype = (int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "numberpe");
            int *numberpe = (int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "mi");
            int *mi = (int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "mi_total");
            long int *mi_total = (long int *)var ->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "mi_offset");
            long int *mi_offset = (long int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "ntracer");
            double *ntracer = (double *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "etracer_i0");
            double *etracer_i0 = (double *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "misum");
            double *misum = (double *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "zion");
            double *zion = (double *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "ntracer_e");
            double *ntracer_e = (double *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "etracer_e0");
            double *etracer_e0 = (double *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "mesum");
            double *mesum = (double *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "me");
            int *me = (int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "me_total");
            long int *me_total = (long int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "me_offset");
            long int *me_offset = (long int *)var->data;

            var = adios_read_shm_get_var_byname(&pgs[i], "zeon");
            double *zeon = (double *)var->data;
 
            inq_var_end(restart_read_prof, time_index);

            prof_point_start(&local_min_prof, 11, time_index);
     
            // local analysis and aggregation
            rt = Analyp_combine(zion, *mi, gts_local_features, first_pg);

            // local analysis and aggregation
            rt = Analyp_combine(zeon, *me, gts_local_features_e, first_pg);
            first_pg = 0;
            
            prof_point_end(local_min_prof, 11, time_index);
        }

        // cross pg processing

        prof_point_start(&global_min_prof, 12, time_index);

        // global aggregation among analysis processes
        global_minmax(gts_local_features,  
                      gts_global_features,
                      comm
                     );

        global_minmax(gts_local_features_e,
                      gts_global_features_e,
                      comm
                     );

        prof_point_end(global_min_prof, 12, time_index);

/////////////////////////////////////////////////////////////
//             Histogram                                   //
/////////////////////////////////////////////////////////////

        prof_point_start(&histogram_prof, 15, time_index);
        
        // zion
        rc = histogram_analysis_init(gts_local_features, 
                                     gts_global_features,
                                     &histograms,
                                     rank,
                                     (time_index==0)?1:0
                                    );

        rc = histogram_analysis_loop(&histograms,
                                     pgs,
                                     f->num_pgs, 
                                     1, // zion                                     
                                     time_index*2, // every 2 simulation timestep
                                     rank,
                                     comm
                                    );
        // zeon
        rc = histogram_analysis_init(gts_local_features_e, 
                                     gts_global_features_e,
                                     &histograms_e,
                                     rank,
                                     (time_index==0)?1:0
                                    );

        rc = histogram_analysis_loop(&histograms_e,
                                     pgs,
                                     f->num_pgs, 
                                     0, // zeon                                     
                                     time_index*2, // every 2 simulation timestep
                                     rank,
                                     comm
                                    );

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
                                  f->num_pgs, 
                                  1,                            
                                  rank,
                                  comm
                                 );

        // zeon
        rc = range_query_analysis(gts_global_features_e,
                                  pgs,
                                  f->num_pgs, 
                                  0,                            
                                  rank,
                                  comm
                                 );

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

        prof_point_end(histogram_2d_prof, 14, time_index);
/////////////////////////////////////////////////////////////
//             End of 2D Histogram                         //
/////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////
//             write query results                         //
/////////////////////////////////////////////////////////////

#if 0
        int mi_total, mi_offset, me_total, me_offset;

        int64_t query_file_handle, query_group_handle;
        char fname[20];
        char *mode;
        uint64_t data_size, total_size;

        sprintf(fname, "query.%d.bp", time_index);
        char *gname = "rangequery";
        if(time_index == 0) {
            mode = "w";
        }
        else {
            mode = "a";
        }
        rc = adios_open(query_file_handle, gname, fname, mode, &comm);
        data_size = sizeof(int) +
                    sizeof(int) +
                    sizeof(int) +
                    sizeof(int) +
                    sizeof(long int) +
                    sizeof(long int) +
                    sizeof(double) +
                    sizeof(double) +
                    sizeof(double) +
                    7 * my_mi * sizeof(double) +
                    sizeof(double) +
                    sizeof(double) +
                    sizeof(double) + 
                    sizeof(int) +
                    sizeof(long int) +
                    sizeof(long int) +
                    7 * my_me * sizeof(double);
        adios_group_size(query_file_handle, data_size, &total_size);

        adios_write(query_file_handle, "rank", &rank);
        adios_write(query_file_handle, "numbtasks", &numtasks);
        adios_write(query_file_handle, "time_index", &time_index);
        adios_write(query_file_handle, "mi", my_mi);
        adios_write(query_file_handle, "mi_total", void * var);
        adios_write(query_file_handle, "mi_offset", void * var);
        adios_write(query_file_handle, "zion", qu_mi);
        adios_write(query_file_handle, "me", my_me);
        adios_write(query_file_handle, "me_total", void * var);
        adios_write(query_file_handle, "me_offset", void * var);
        adios_write(query_file_handle, "zeon", qu_me);

        adios_close(query_file_handle);
#endif

/////////////////////////////////////////////////////////////
//             end of write query results                  //
/////////////////////////////////////////////////////////////


        gclose_start(restart_read_prof, time_index);

        // release pgs of current timestep
        for(i = 0; i < f->num_pgs; i ++) {
            adios_read_shm_release_pg(&pgs[i]);
        }

        gclose_end(restart_read_prof, time_index);

        fclose_start(restart_read_prof, time_index);

        // proceed to the next timestep
        adios_read_shm_advance_timestep(f);

        fclose_end(restart_read_prof, time_index);

        cycle_end(time_index);

        time_index ++;
    }

    simulation_main_loop_end();

    // finalize before closing file

    // close
    prof_point_start(&fclose_prof, 2, 0);

    adios_read_shm_fclose(f);

    prof_point_end(fclose_prof, 2, 0);

    // phase 3: finalize
    MPI_Barrier(comm);

//    adios_finalize(rank);

    MPI_Barrier(comm);
    adios_read_finalize_start();

    adios_read_shm_finalize();

    adios_read_finalize_end();

    simulation_end();
    finalize_prof_read_all();   
    finalize_prof_simulation();

    MPI_Finalize();
    return 0;
}


