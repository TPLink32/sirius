#ifndef _TIMING_H
#define _TIMEING_H 1

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
#include "adios_read.h"

#define MAX_IO_COUNT 1000
#define MAX_PRINT_BUF_SIZE 1*1024*1024
#define MAX_CYCLE_COUNT 1000
#define MAX_PROF_POINT_PER_CYCLE 20

/*
 * Profiling data structure
 *  - simulaiton-related: init/main loop/finalize timing; cycle timing
 *  - adios write timing
 *  - adios read timing
 */
typedef struct _cycle_timing_info {
    int cycle;
    double cycle_start_time;
    double cycle_end_time;
    double cycle_time;
} cycle_timing_info;

/*
 * arbitrary profiling point
 */
typedef struct _prof_point_info {
    int tag;
    int current_cycle;
    int counts;
    int *cycles;
    double *start_time;
    double *end_time;
    double *time;
} prof_point_info;

typedef struct _simulation_timing_info {
    MPI_Comm communicator;
    int comm_size;
    int myid;
    double sim_start_time;
    double sim_end_time;
    double main_loop_start_time;
    double main_loop_end_time;

    double adios_init_start_time;
    double adios_init_end_time;
    double adios_finalize_start_time;
    double adios_finalize_end_time;

    double adios_read_init_start_time;
    double adios_read_init_end_time;
    double adios_read_finalize_start_time;
    double adios_read_finalize_end_time;

    int max_cycle_count;
    int cycle_count;
    cycle_timing_info *cycle_timing;

    int max_prof_point_count;
    int prof_point_count;
    prof_point_info *prof_points;

    char *prof_file_name;
    FILE *prof_file;
} simulation_timing_info;

typedef struct _timing_info_write {
    int cycle;
    double open_start_time;
    double open_end_time;
    double open_time;
    double grp_size_start_time;
    double grp_size_end_time;
    double grp_size_time;
    double write_start_time;
    double write_end_time;
    double write_time;
    double close_start_time;
    double close_end_time;
    double close_time;
} timing_info_write;

// added by Jianting
typedef struct _timing_info_read {
    int cycle;
    double fopen_start_time;
    double fopen_end_time;
    double fopen_time;
    double read_start_time;
    double read_end_time;
    double read_time;
    double gopen_start_time;
    double gopen_end_time;
    double gopen_time;
    double fclose_start_time;
    double fclose_end_time;
    double fclose_time;
    double gclose_start_time;
    double gclose_end_time;
    double gclose_time;
    double inq_file_start_time;
    double inq_file_end_time;
    double inq_file_time;
    double inq_grp_start_time;
    double inq_grp_end_time;
    double inq_grp_time;
    double inq_var_start_time;
    double inq_var_end_time;
    double inq_var_time;
} timing_info_read;

typedef struct _group_prof_info_write {
    struct adios_group_struct *adios_group_id;
    MPI_Comm communicator;
    int comm_size;
    int myid;
    int max_io_count;
    int io_count;
    timing_info_write *timing;
} group_prof_info_write;

typedef struct _group_prof_info_read {
    char *file_name;
    ADIOS_FILE *adios_file_id; // TODO: let's assume one group per file
    MPI_Comm communicator;
    int comm_size;
    int myid;
    int max_io_count;
    int io_count;
    timing_info_read *timing;
} group_prof_info_read;

typedef struct _io_prof_setting_write {
    char *prof_file_name;
    MPI_File prof_file_ptr;
    int num_groups;
    group_prof_info_write *gp_prof_info;
} io_prof_setting_write;

typedef struct _io_prof_setting_read {
    char *prof_file_name;
    MPI_File prof_file_ptr;
    int num_groups;
    group_prof_info_read *gp_prof_info;
} io_prof_setting_read;

/*
 * get local timestamp
 */
double get_timestamp();

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
                  );

#endif

