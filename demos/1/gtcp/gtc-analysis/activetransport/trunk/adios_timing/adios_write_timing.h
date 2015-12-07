#ifndef ADIOS_TIMING_H
#define ADIOS_TIMING_H 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _NOMPI
/* Sequential processes can use the library compiled with -D_NOMPI */
#include "mpidummy.h"
#else
/* Parallel applications should use MPI to communicate file info and slices of data */
#include "mpi.h"
#endif

/*
 * initialize profiling 
 *
 * Fortran interface
 */
int init_prof_all_write_(char *prof_file_name, int prof_file_name_size);

/*
 * initialize profiling
 *
 * C interface
 */
int init_prof_all_write(char *prof_file_name);

/*
 * record open start time for specified group
 *
 * Fortran interface
 */
void open_start_for_group_(long long *gp_prof_handle, char *group_name, int *cycle, int *gp_name_size);

/*
 * record open start time for specified group
 *
 * C interface
 */
void open_start_for_group(long long *gp_prof_handle, char *group_name, int cycle);

/*
 * record open end time for specified group
 *
 * Fortran interface
 */
void open_end_for_group_(long long *gp_prof_handle, int *cycle);

/*
 * record open end time for specified group
 *
 * C interface
 */
void open_end_for_group(long long gp_prof_handle, int cycle);

/*
 * record group_size start time for specified group
 *
 * Fortran interface
 */
void group_size_start_for_group_(long long *gp_prof_handle, int *cycle);

/*
 * record group_size start time for specified group
 *
 * C interface
 */
void group_size_start_for_group(long long gp_prof_handle, int cycle);

/*
 * record group_size end time for specified group
 *
 * Fortran interface
 */
void group_size_end_for_group_(long long *gp_prof_handle, int *cycle);

/*
 * record group_size end time for specified group
 *
 * C interface
 */
void group_size_end_for_group(long long gp_prof_handle, int cycle);

/*
 * record write start time for specified group
 *
 * Fortran interface
 */
void write_start_for_group_(long long *gp_prof_handle, int *cycle);

/*
 * record write start time for specified group
 *
 * C interface
 */
void write_start_for_group(long long gp_prof_handle, int cycle);

/*
 * record write end time for specified group
 *
 * Fortran interface
 */
void write_end_for_group_(long long *gp_prof_handle, int *cycle);

/*
 * record write end time for specified group
 *
 * C interface
 */
void write_end_for_group(long long gp_prof_handle, int cycle);

/*
 * record close start time for specified group
 *
 * Fortran interface
 */
void close_start_for_group_(long long *gp_prof_handle, int *cycle);

/*
 * record close start time for specified group
 *
 * C interface
 */
void close_start_for_group(long long gp_prof_handle, int cycle);

/*
 * record close end time for specified group
 *
 * Fortran interface
 */
void close_end_for_group_(long long *gp_prof_handle, int *cycle);

/*
 * record close end time for specified group
 *
 * C interface
 */
void close_end_for_group(long long gp_prof_handle, int cycle);

/*
 * Report timing info for all groups
 *
 * Fortran interface  
 */
int finalize_prof_write_all_();

/*
 * Report timing info for all groups
 *
 * C interface
 */
int finalize_prof_write_all();

#ifdef __cplusplus
}
#endif

#endif


