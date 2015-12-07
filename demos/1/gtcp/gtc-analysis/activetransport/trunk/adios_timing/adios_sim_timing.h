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
 * record simulation start time (call it after MPI_Init)
 *
 * Fortran interface
 */
void simulation_start_();

/*
 * record simulation start time (call it after MPI_Init)
 *
 * C interface
 */
void simulation_start();

/*
 * record simulation end time (call it before MPI_Finalize)
 *
 * Fortran interface
 */
void simulation_end_();

/*
 * record simulation end time (call it before MPI_Finalize)
 *
 * C interface
 */
void simulation_end();

/*
 * record simulation main loop start time
 *
 * Fortran interface
 */
void simulation_main_loop_start_();

/*
 * record simulation main loop start time
 *
 * C interface
 */
void simulation_main_loop_start();

/*
 * record simulation main loop end time
 *
 * Fortran interface
 */
void simulation_main_loop_end_();

/*
 * record simulation main loop end time
 *
 * C interface
 */
void simulation_main_loop_end();

/*
 * record adios_init() call start time
 *
 * Fortran interface
 */
void adios_init_start_();

/*
 * record adios_init() call start time
 *
 * C interface
 */
void adios_init_start();

/*
 * record adios_init() call end time
 *
 * Fortran interface
 */
void adios_init_end_();

/*
 * record adios_init() call end time
 *
 * C interface
 */
void adios_init_end();

/*
 * record adios_finalize() call start time
 *
 * Fortran interface
 */
void adios_finalize_start_();

/*
 * record adios_finalize() call start time
 *
 * C interface
 */
void adios_finalize_start();

/*
 * record adios_finalize() call end time
 *
 * Fortran interface
 */
void adios_finalize_end_();

/*
 * record adios_finalize() call end time
 *
 * C interface
 */
void adios_finalize_end();

/*
 * record adios_read_init() call start time
 *
 * Fortran interface
 */
void adios_read_init_start_();

/*
 * record adios_read_init() call start time
 *
 * C interface
 */
void adios_read_init_start();

/*
 * record adios_read_init() call end time
 *
 * Fortran interface
 */
void adios_read_init_end_();

/*
 * record adios_read_init() call end time
 *
 * C interface
 */
void adios_read_init_end();

/*
 * record adios_read_finalize() call start time
 *
 * Fortran interface
 */
void adios_read_finalize_start_();

/*
 * record adios_read_finalize() call start time
 *
 * C interface
 */
void adios_read_finalize_start();

/*
 * record adios_read_finalize() call end time
 *
 * Fortran interface
 */
void adios_read_finalize_end_();

/*
 * record adios_read_finalize() call end time
 *
 * C interface
 */
void adios_read_finalize_end();

/*
 * initialize profiling 
 *
 */
int init_prof_simulation(char *prof_file_name
             ,int max_cycle_count
             ,int max_prof_point_count
             ,MPI_Comm comm
             );

/*
 * Fortran interface for init_prof()
 *
 */
int init_prof_simulation_(char *prof_file_name
              ,int *max_cycle_count
              ,int *max_prof_point_count
              ,MPI_Fint *comm
              ,int *pfile_name_size
              );

/*
 * record start time of a simulation cycle
 *
 * Fortran interface 
 */
void cycle_start_(int *cycle);

/*
 * record start time of a simulation cycle
 *
 * C interface
 */
void cycle_start(int cycle);

/*
 * record end time of a simulation cycle
 *
 * Fortran interface 
 */
void cycle_end_(int *cycle);

/*
 * record end time of a simulation cycle
 *
 * C interface
 */
void cycle_end(int cycle);

/*
 * report profling results
 *
 * Fortran interface
 */
int finalize_prof_simulation_();

/*
 * report profling results
 *
 * C interface
 */
int finalize_prof_simulation();

/*
 * define an arbitrary tracing  point and record start time
 *
 * C interface
 */
void prof_point_start(long long *prof_handle, int tag, int cycle);

/*
 * define an arbitrary tracing  point and record start time
 *
 * Fortran interface
 */
void prof_point_start_(long long *gp_prof_handle, int *tag, int *cycle);

/*
 * record end time of a profiling point
 * 
 * C interface
 */
void prof_point_end(long long prof_handle, int tag, int cycle);

/*
 * record end time of a profiling point
 *
 * Fortran interface
 */
void prof_point_end_(long long *prof_handle, int *tag, int *cycle);

#ifdef __cplusplus
}
#endif

#endif


