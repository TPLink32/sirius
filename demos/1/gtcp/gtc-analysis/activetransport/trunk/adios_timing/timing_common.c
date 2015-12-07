#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include "mpi.h"

#include "adios.h"
//#include "adios_transport_hooks.h"
//#include "adios_internals.h"
//#include "adios_read.h"

#include "timing_common.h"

/*
 * the following variables have been deprecated in
 * ADIOS; so just set them to use default MPI values 
 */
MPI_Comm adios_mpi_comm_world_timing = MPI_COMM_WORLD;
MPI_Comm adios_mpi_comm_self = MPI_COMM_SELF;

/*
 * get local timestamp
 */
double get_timestamp()
{
#ifdef TIMER_MPI_WTIME 
    return MPI_Wtime();
#else   
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);

    double realtime = timestamp.tv_sec + timestamp.tv_usec / 1.0e6;
    return realtime;
#endif
}

/* 
 * perform allreduces necessary to gather statistics on a single double 
 * value recorded across a comm.
 * 
 * code borrowed from mpi-tile-io package
 */
void get_time_dist(MPI_Comm comm
                  ,double *time
                  ,double *min
                  ,double *max
                  ,double *mean
                  ,double *var
                  )
{
    int nr_procs;
    double sq_time_part, sum, sum_sq;
    
    MPI_Comm_size(comm, &nr_procs);

    MPI_Allreduce(time, max, 1, MPI_DOUBLE, MPI_MAX, comm);
    MPI_Allreduce(time, min, 1, MPI_DOUBLE, MPI_MIN, comm);
    MPI_Allreduce(time, &sum, 1, MPI_DOUBLE, MPI_SUM, comm);

    *mean = sum / nr_procs;

    /* calculate our part of the variance */
    sq_time_part = *time - *mean;
    sq_time_part = sq_time_part * sq_time_part;
    MPI_Allreduce(&sq_time_part, &sum_sq, 1, MPI_DOUBLE, MPI_SUM, comm);

    if (nr_procs > 1) {
        *var = sqrt( sum_sq / ((double) nr_procs - 1.0) ); // sample standard deviation
    }
    else {
        *var = 0.0;
    }

    return;
}


