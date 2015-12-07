#ifndef ADIOS_READ_TIMING_H
#define ADIOS_READ_TIMING_H 1

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

#include "adios_read.h"

/*
 * ADIOS Read API profiling
 *
 * Fortran interface
 */
//added by Jianting record start and end time for gopen gclose
void fopen_start_(long long *gp_prof_handle, char *file_name, int *cycle, MPI_Fint *comm, int *f_name_size);
void fopen_end_(long long *gp_prof_handle, long long *adios_file_handle, int *cycle);
void gopen_start_(long long *gp_prof_handle, int *cycle);
void gopen_end_(long long *gp_prof_handle, int *cycle);
void gclose_start_(long long *gp_prof_handle, int *cycle);
void gclose_end_(long long *gp_prof_handle, int *cycle);
void inq_file_start_(long long *gp_prof_handle, int *cycle);
void inq_file_end_(long long *gp_prof_handle, int *cycle);
void inq_grp_start_(long long *gp_prof_handle, int *cycle);
void inq_grp_end_(long long *gp_prof_handle, int *cycle);
void inq_var_start_(long long *gp_prof_handle, int *cycle);
void inq_var_end_(long long *gp_prof_handle, int *cycle);
void read_start_(long long *gp_prof_handle, int *cycle);
void read_end_(long long *gp_prof_handle, int *cycle);
void fclose_start_(long long *gp_prof_handle, int *cycle);
void fclose_end_(long long *gp_prof_handle, int *cycle);
int init_prof_read_all_();
int finalize_prof_read_all_();

/*
 * ADIOS Read API profiling
 *
 * C interface
 */
void fopen_start(long long *gp_prof_handle, char *file_name, int cycle, MPI_Comm comm);
void fopen_end(long long gp_prof_handle, long long adios_file_handle, int cycle);
void gopen_start(long long gp_prof_handle, int cycle);
void gopen_end(long long gp_prof_handle, int cycle);
void gclose_start(long long gp_prof_handle, int cycle);
void gclose_end(long long gp_prof_handle, int cycle);
void inq_file_start(long long gp_prof_handle, int cycle);
void inq_file_end(long long gp_prof_handle, int cycle);
void inq_grp_start(long long gp_prof_handle, int cycle);
void inq_grp_end(long long gp_prof_handle, int cycle);
void inq_var_start(long long gp_prof_handle, int cycle);
void inq_var_end(long long gp_prof_handle, int cycle);
void read_start(long long gp_prof_handle, int cycle);
void read_end(long long gp_prof_handle, int cycle);
void fclose_start(long long gp_prof_handle, int cycle);
void fclose_end(long long gp_prof_handle, int cycle);
int init_prof_read_all();
int finalize_prof_read_all();

#ifdef __cplusplus
}
#endif

#endif


