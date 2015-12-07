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

#include "timing_common.h"

static io_prof_setting_read profile_read;

extern MPI_Comm adios_mpi_comm_world_timing;
extern MPI_Comm adios_mpi_comm_self;

/*
 * find profiling struct by group id for reading
 */
group_prof_info_read *get_prof_by_group_name_read(char *file_name)
{
    int i = 0;
    for(; i < profile_read.num_groups; i ++) {
        if(!strcmp(profile_read.gp_prof_info[i].file_name, file_name)) {
            return &profile_read.gp_prof_info[i];
        }
    }

    return NULL;
}

/*
 * initialize profiling for reading
 *
 */
int init_prof_read_all()
{
    profile_read.prof_file_name = NULL;
    profile_read.num_groups = 0;
    profile_read.gp_prof_info = NULL;
    return 0;
}

int init_prof_read_all_() {
    init_prof_read_all();
}

//added by Jianting
void fopen_start(long long *gp_prof_handle, char *file_name, int cycle, MPI_Comm comm)
{
    group_prof_info_read *gp_prof = get_prof_by_group_name_read(file_name);

    if(!gp_prof) { // this is the first time to open this file
        profile_read.num_groups ++; 
        group_prof_info_read *temp = (group_prof_info_read *) 
            realloc(profile_read.gp_prof_info, profile_read.num_groups * sizeof(group_prof_info_read));
        if(!temp) {
            fprintf(stderr, "ADIOS Profiling: cannot allocate memory.\n");
            exit(-1);
        } 
        profile_read.gp_prof_info = temp;

        gp_prof = &profile_read.gp_prof_info[profile_read.num_groups-1];
        gp_prof->file_name = strdup(file_name);
        gp_prof->adios_file_id = NULL; // will be filled in in fopen_end_() function
        gp_prof->communicator = comm;
        gp_prof->comm_size = 0;
        gp_prof->myid = 0;

        // TODO: hardcode max_io_count now; later change to use link list or set by env variable
        gp_prof->max_io_count = MAX_IO_COUNT;
        gp_prof->io_count = 0;

        // NOTE:
        // timing_info is initialized with all bits set to 0.
        // Therefore, timing[i].fopen_start_time == 0 means this process didn't do IO at that cycle
        gp_prof->timing = (timing_info_read *) calloc(gp_prof->max_io_count, sizeof(timing_info_read));
    }

    *gp_prof_handle = (long long)gp_prof;

    if(gp_prof->io_count < gp_prof->max_io_count) {
        gp_prof->timing[gp_prof->io_count].cycle = cycle;
        gp_prof->timing[gp_prof->io_count].fopen_start_time = get_timestamp();
    }
    else {
        fprintf(stderr, "ADIOS Profiling: io count exceeds max limit (%d)\n", gp_prof->max_io_count);
        exit(-1);
    }
}

void fopen_start_(long long *gp_prof_handle, char *file_name, int *cycle, MPI_Fint *comm, int *f_name_size)
{
    fopen_start(gp_prof_handle, file_name, *cycle, MPI_Comm_f2c(*comm));
}

//added by Jianting
void fopen_end(long long gp_prof_handle, long long adios_file_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    // fill in the ADIOS_FILE handle
    if(adios_file_handle) {
        gp_prof->adios_file_id = (ADIOS_FILE *)adios_file_handle;
    }
    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].fopen_end_time = get_timestamp();

    // accumulate open time because user code may call adios_fopen() multiple times
    // to test file availability
    gp_prof->timing[gp_prof->io_count].fopen_time += gp_prof->timing[gp_prof->io_count].fopen_end_time
                                              - gp_prof->timing[gp_prof->io_count].fopen_start_time;
}

void fopen_end_(long long *gp_prof_handle, long long *adios_file_handle, int *cycle)
{
    fopen_end(*gp_prof_handle, *adios_file_handle, *cycle);
}

//added by Jianting
void read_start(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].read_start_time = get_timestamp();
}

void read_start_(long long *gp_prof_handle, int *cycle)
{
    read_start(*gp_prof_handle, *cycle);
}

//added by Jianting
void read_end(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].read_end_time = get_timestamp();
    
    // added by zf2: note that here we accumulate the read time since user code may do something else
    // between adios_read_var() calls
    gp_prof->timing[gp_prof->io_count].read_time += gp_prof->timing[gp_prof->io_count].read_end_time
                                              - gp_prof->timing[gp_prof->io_count].read_start_time;
}

void read_end_(long long *gp_prof_handle, int *cycle)
{
    read_end(*gp_prof_handle, *cycle);
}

//added by Jianting
void fclose_start(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].fclose_start_time = get_timestamp();
}

void fclose_start_(long long *gp_prof_handle, int *cycle)
{
    fclose_start(*gp_prof_handle, *cycle);
}

//added by Jianting
void fclose_end(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].fclose_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].fclose_time = gp_prof->timing[gp_prof->io_count].fclose_end_time
                                              - gp_prof->timing[gp_prof->io_count].fclose_start_time;
    gp_prof->io_count ++;
}

void fclose_end_(long long *gp_prof_handle, int *cycle)
{
    fclose_end(*gp_prof_handle, *cycle);
}

//added by Jianting gopen and gclose timing
void gopen_start(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
	fprintf(stderr, "ADIOS Profiling: unknow group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].gopen_start_time = get_timestamp();
}

void gopen_start_(long long *gp_prof_handle, int *cycle)
{
    gopen_start(*gp_prof_handle, *cycle);
}

void gopen_end(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].gopen_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].gopen_time = gp_prof->timing[gp_prof->io_count].gopen_end_time
                                              - gp_prof->timing[gp_prof->io_count].gopen_start_time;
}

void gopen_end_(long long *gp_prof_handle, int *cycle)
{
    gopen_end(*gp_prof_handle, *cycle);
}

void gclose_start(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unknow group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].gclose_start_time = get_timestamp();
}

void gclose_start_(long long *gp_prof_handle, int *cycle)
{
    gclose_start(*gp_prof_handle, *cycle);
}

void gclose_end(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].gclose_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].gclose_time = gp_prof->timing[gp_prof->io_count].gclose_end_time
                                              - gp_prof->timing[gp_prof->io_count].gclose_start_time;
}

void gclose_end_(long long *gp_prof_handle, int *cycle)
{
    gclose_end(*gp_prof_handle, *cycle);
}

void inq_file_start(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unknow group\n");
    }
    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].inq_file_start_time = get_timestamp();
}

void inq_file_start_(long long *gp_prof_handle, int *cycle)
{
    inq_file_start(*gp_prof_handle, *cycle);
}

void inq_file_end(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].inq_file_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].inq_file_time = gp_prof->timing[gp_prof->io_count].inq_file_end_time
                                              - gp_prof->timing[gp_prof->io_count].inq_file_start_time;
}

void inq_file_end_(long long *gp_prof_handle, int *cycle)
{
    inq_file_end(*gp_prof_handle, *cycle);
}

void inq_grp_start(long long gp_prof_handle, int cycle) 
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unknow group\n");
    }
    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].inq_grp_start_time = get_timestamp();
}

void inq_grp_start_(long long *gp_prof_handle, int *cycle)
{
    inq_grp_start(*gp_prof_handle, *cycle);
}

void inq_grp_end(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].inq_grp_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].inq_grp_time = gp_prof->timing[gp_prof->io_count].inq_grp_end_time
                                              - gp_prof->timing[gp_prof->io_count].inq_grp_start_time;
}

void inq_grp_end_(long long *gp_prof_handle, int *cycle)
{
    inq_grp_end(*gp_prof_handle, *cycle);
}

void inq_var_start(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unknow group\n");
    }
    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].inq_var_start_time = get_timestamp();
}

void inq_var_start_(long long *gp_prof_handle, int *cycle)
{
    inq_var_start(*gp_prof_handle, *cycle);
}

void inq_var_end(long long gp_prof_handle, int cycle)
{
    group_prof_info_read *gp_prof = (group_prof_info_read *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].inq_var_end_time = get_timestamp();

    // accumulate the time since adios_inq_var() may be called several times
    gp_prof->timing[gp_prof->io_count].inq_var_time += gp_prof->timing[gp_prof->io_count].inq_var_end_time
                                              - gp_prof->timing[gp_prof->io_count].inq_var_start_time;
}

void inq_var_end_(long long *gp_prof_handle, int *cycle)
{
    inq_var_end(*gp_prof_handle, *cycle);
}

//added by Jianting
int finalize_prof_read_all()
{
    int i;
    int g;
    int rc;
    int global_rank;
    int global_size;
    MPI_Comm_size(adios_mpi_comm_world_timing, &global_size);
    MPI_Comm_rank(adios_mpi_comm_world_timing, &global_rank);
    char print_buf[MAX_PRINT_BUF_SIZE];
    char *cur = print_buf;
    int num_written = 0;
    int print_flag = 0;

/*
    // adios_read_init and adios_read_finalize timing
    double adios_read_init_time = profile_read.adios_read_init_end_time - profile_read.adios_read_init_start_time;
    double adios_read_fin_time = profile_read.adios_read_finalize_end_time - profile_read.adios_read_finalize_start_time;

    double adios_read_init_time_min, adios_read_init_time_max, adios_read_init_time_mean, adios_read_init_time_var;
    double adios_read_init_stime_min, adios_read_init_stime_max, adios_read_init_stime_mean, adios_read_init_stime_var;
    double adios_read_init_etime_min, adios_read_init_etime_max, adios_read_init_etime_mean, adios_read_init_etime_var;
    double adios_read_fin_time_min, adios_read_fin_time_max, adios_read_fin_time_mean, adios_read_fin_time_var;
    double adios_read_fin_stime_min, adios_read_fin_stime_max, adios_read_fin_stime_mean, adios_read_fin_stime_var;
    double adios_read_fin_etime_min, adios_read_fin_etime_max, adios_read_fin_etime_mean, adios_read_fin_etime_var;

    get_time_dist(adios_mpi_comm_world_timing
                 ,&adios_read_init_time
                 ,&adios_read_init_time_min
                 ,&adios_read_init_time_max
                 ,&adios_read_init_time_mean
                 ,&adios_read_init_time_var
                 );

    get_time_dist(adios_mpi_comm_world_timing
                 ,&(profile_read.adios_read_init_start_time)
                 ,&adios_read_init_stime_min
                 ,&adios_read_init_stime_max
                 ,&adios_read_init_stime_mean
                 ,&adios_read_init_stime_var
                 );

    get_time_dist(adios_mpi_comm_world_timing
                 ,&(profile_read.adios_read_init_end_time)
                 ,&adios_read_init_etime_min
                 ,&adios_read_init_etime_max
                 ,&adios_read_init_etime_mean
                 ,&adios_read_init_etime_var
                 );

    get_time_dist(adios_mpi_comm_world_timing
                 ,&adios_read_fin_time
                 ,&adios_read_fin_time_min
                 ,&adios_read_fin_time_max
                 ,&adios_read_fin_time_mean
                 ,&adios_read_fin_time_var
                 );

    get_time_dist(adios_mpi_comm_world_timing
                 ,&(profile_read.adios_read_finalize_start_time)
                 ,&adios_read_fin_stime_min
                 ,&adios_read_fin_stime_max
                 ,&adios_read_fin_stime_mean
                 ,&adios_read_fin_stime_var
                 );

    get_time_dist(adios_mpi_comm_world_timing
                 ,&(profile_read.adios_read_finalize_end_time)
                 ,&adios_read_fin_etime_min
                 ,&adios_read_fin_etime_max
                 ,&adios_read_fin_etime_mean
                 ,&adios_read_fin_etime_var
                 );
*/

    for(g = 0; g < profile_read.num_groups; g ++) {
        group_prof_info_read *gp_prof = &profile_read.gp_prof_info[g];

        if(gp_prof->communicator == MPI_COMM_NULL) {
            // if group communicator is not specified in config.xml, then assume
            // this processes did IO independently. This complies with adios_mpi
            // and adios_mpi_cio convention 
            gp_prof->communicator = adios_mpi_comm_self;
        }

        MPI_Comm_size(gp_prof->communicator, &gp_prof->comm_size);
        MPI_Comm_rank(gp_prof->communicator, &gp_prof->myid);      
        memset(print_buf, 0, MAX_PRINT_BUF_SIZE);
        cur = print_buf;
        print_flag = 0;
 
        if(global_rank == 0) {
            print_flag = 1;
            num_written = sprintf(cur, "Timing results: \n");
            cur += num_written;
            num_written = sprintf(cur, "Operations    : \t %-18s \t %-18s \t %-18s \t %-18s\n",
                "min", "max", "mean", "var");
            cur += num_written;
        }

        for(i = 0; i < gp_prof->io_count; i ++) {
            // gather timing info to proc 0
            double total_io_time, total_io_time_min, total_io_time_max, total_io_time_mean, total_io_time_var;
//added by Jianting
	    double fopen_time_min, fopen_time_max, fopen_time_mean, fopen_time_var;
	    double fopen_stime_min, fopen_stime_max, fopen_stime_mean, fopen_stime_var;
	    double fopen_etime_min, fopen_etime_max, fopen_etime_mean, fopen_etime_var;
	    double read_time_min, read_time_max, read_time_mean, read_time_var;
	    double read_stime_min, read_stime_max, read_stime_mean, read_stime_var;
	    double read_etime_min, read_etime_max, read_etime_mean, read_etime_var;
	    double fclose_time_min, fclose_time_max, fclose_time_mean, fclose_time_var;
	    double fclose_stime_min, fclose_stime_max, fclose_stime_mean, fclose_stime_var;
	    double fclose_etime_min, fclose_etime_max, fclose_etime_mean, fclose_etime_var;
	    double gopen_time_min, gopen_time_max, gopen_time_mean, gopen_time_var;
	    double gopen_stime_min, gopen_stime_max, gopen_stime_mean, gopen_stime_var;
	    double gopen_etime_min, gopen_etime_max, gopen_etime_mean, gopen_etime_var;
	    double gclose_time_min, gclose_time_max, gclose_time_mean, gclose_time_var;
	    double gclose_stime_min, gclose_stime_max, gclose_stime_mean, gclose_stime_var;
	    double gclose_etime_min, gclose_etime_max, gclose_etime_mean, gclose_etime_var;
            double inq_file_time_min, inq_file_time_max, inq_file_time_mean, inq_file_time_var;
            double inq_file_stime_min, inq_file_stime_max, inq_file_stime_mean, inq_file_stime_var;
            double inq_file_etime_min, inq_file_etime_max, inq_file_etime_mean, inq_file_etime_var;
            double inq_grp_time_min, inq_grp_time_max, inq_grp_time_mean, inq_grp_time_var;
            double inq_grp_stime_min, inq_grp_stime_max, inq_grp_stime_mean, inq_grp_stime_var;
            double inq_grp_etime_min, inq_grp_etime_max, inq_grp_etime_mean, inq_grp_etime_var;
            double inq_var_time_min, inq_var_time_max, inq_var_time_mean, inq_var_time_var;

            // since there may be only part of processes within the comm which actually did IO,
            // we need to first check how many processes did IO
            int did_io = (gp_prof->timing[i].fopen_start_time != 0)? 1 : 0;
            MPI_Comm comm_io;
            rc = MPI_Comm_split(gp_prof->communicator, did_io, gp_prof->myid, &comm_io);
            if(rc != MPI_SUCCESS) {
                int error_class; 
                MPI_Error_class(rc, &error_class);
                fprintf(stderr, "ADIOS Profiling: Error %d returned by MPI_Comm_split()\n", 
                    error_class);
                return -1;
            }

            int comm_io_rank, comm_io_size;
            MPI_Comm_rank(comm_io, &comm_io_rank);
            MPI_Comm_size(comm_io, &comm_io_size);
            
            if(did_io) {
                get_time_dist(comm_io
                              ,&gp_prof->timing[i].fopen_time
                              ,&fopen_time_min
                              ,&fopen_time_max
                              ,&fopen_time_mean
                              ,&fopen_time_var
                              );
                get_time_dist(comm_io
                              ,&gp_prof->timing[i].fopen_start_time
                              ,&fopen_stime_min
                              ,&fopen_stime_max
                              ,&fopen_stime_mean
                              ,&fopen_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].fopen_end_time
                              ,&fopen_etime_min
                              ,&fopen_etime_max
                              ,&fopen_etime_mean
                              ,&fopen_etime_var
                              );
                get_time_dist(comm_io
                              ,&gp_prof->timing[i].gopen_time
                              ,&gopen_time_min
                              ,&gopen_time_max
                              ,&gopen_time_mean
                              ,&gopen_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].gopen_start_time
                              ,&gopen_stime_min
                              ,&gopen_stime_max
                              ,&gopen_stime_mean
                              ,&gopen_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].gopen_end_time
                              ,&gopen_etime_min
                              ,&gopen_etime_max
                              ,&gopen_etime_mean
                              ,&gopen_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].read_time
                              ,&read_time_min
                              ,&read_time_max
                              ,&read_time_mean
                              ,&read_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].read_start_time
                              ,&read_stime_min
                              ,&read_stime_max
                              ,&read_stime_mean
                              ,&read_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].read_end_time
                              ,&read_etime_min
                              ,&read_etime_max
                              ,&read_etime_mean
                              ,&read_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].fclose_time
                              ,&fclose_time_min
                              ,&fclose_time_max
                              ,&fclose_time_mean
                              ,&fclose_time_var
                              );    

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].fclose_start_time
                              ,&fclose_stime_min
                              ,&fclose_stime_max
                              ,&fclose_stime_mean
                              ,&fclose_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].fclose_end_time
                              ,&fclose_etime_min
                              ,&fclose_etime_max
                              ,&fclose_etime_mean
                              ,&fclose_etime_var
                              );
                get_time_dist(comm_io
                              ,&gp_prof->timing[i].gclose_time
                              ,&gclose_time_min
                              ,&gclose_time_max
                              ,&gclose_time_mean
                              ,&gclose_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].gclose_start_time
                              ,&gclose_stime_min
                              ,&gclose_stime_max
                              ,&gclose_stime_mean
                              ,&gclose_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].gclose_end_time
                              ,&gclose_etime_min
                              ,&gclose_etime_max
                              ,&gclose_etime_mean
                              ,&gclose_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].inq_file_time
                              ,&inq_file_time_min
                              ,&inq_file_time_max
                              ,&inq_file_time_mean
                              ,&inq_file_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].inq_file_start_time
                              ,&inq_file_stime_min
                              ,&inq_file_stime_max
                              ,&inq_file_stime_mean
                              ,&inq_file_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].inq_file_end_time
                              ,&inq_file_etime_min
                              ,&inq_file_etime_max
                              ,&inq_file_etime_mean
                              ,&inq_file_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].inq_grp_time
                              ,&inq_grp_time_min
                              ,&inq_grp_time_max
                              ,&inq_grp_time_mean
                              ,&inq_grp_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].inq_grp_start_time
                              ,&inq_grp_stime_min
                              ,&inq_grp_stime_max
                              ,&inq_grp_stime_mean
                              ,&inq_grp_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].inq_grp_end_time
                              ,&inq_grp_etime_min
                              ,&inq_grp_etime_max
                              ,&inq_grp_etime_mean
                              ,&inq_grp_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].inq_var_time
                              ,&inq_var_time_min
                              ,&inq_var_time_max
                              ,&inq_var_time_mean
                              ,&inq_var_time_var
                              );

                total_io_time = gp_prof->timing[i].fclose_end_time - gp_prof->timing[i].fopen_start_time; 

                get_time_dist(comm_io
                              ,&total_io_time
                              ,&total_io_time_min
                              ,&total_io_time_max
                              ,&total_io_time_mean
                              ,&total_io_time_var
                              ); 

                if(comm_io_rank == 0) {
                    print_flag = 1;
                    num_written = sprintf(cur, "reported by %d\n", global_rank);
                    cur += num_written;
                    num_written = sprintf(cur, "cycle no \t %d\n", gp_prof->timing[i].cycle);
                    cur += num_written;
                    num_written = sprintf(cur, "io count \t %d\n", i);
                    cur += num_written;

                    num_written = sprintf(cur, "# Fopen        : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            fopen_time_min, fopen_time_max, fopen_time_mean, fopen_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Fopen start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            fopen_stime_min, fopen_stime_max, fopen_stime_mean, fopen_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Fopen end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            fopen_etime_min, fopen_etime_max, fopen_etime_mean, fopen_etime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Gopen        : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            gopen_time_min, gopen_time_max, gopen_time_mean, gopen_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Gopen start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            gopen_stime_min, gopen_stime_max, gopen_stime_mean, gopen_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Gopen end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            gopen_etime_min, gopen_etime_max, gopen_etime_mean, gopen_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Read       : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            read_time_min, read_time_max, read_time_mean, read_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Read start : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            read_stime_min, read_stime_max, read_stime_mean, read_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Read end   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            read_etime_min, read_etime_max, read_etime_mean, read_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Ffclose       : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            fclose_time_min, fclose_time_max, fclose_time_mean, fclose_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Ffclose start : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            fclose_stime_min, fclose_stime_max, fclose_stime_mean, fclose_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Ffclose end   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            fclose_etime_min, fclose_etime_max, fclose_etime_mean, fclose_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Gclose       : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            gclose_time_min, gclose_time_max, gclose_time_mean, gclose_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Gclose start : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            gclose_stime_min, gclose_stime_max, gclose_stime_mean, gclose_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Gclose end   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            gclose_etime_min, gclose_etime_max, gclose_etime_mean, gclose_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Inq_grp      : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            inq_grp_time_min, inq_grp_time_max, inq_grp_time_mean, inq_grp_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Inq_grp start: \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            inq_grp_stime_min, inq_grp_stime_max, inq_grp_stime_mean, inq_grp_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Inq_grp end  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            inq_grp_etime_min, inq_grp_etime_max, inq_grp_etime_mean, inq_grp_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Inq_var      : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            inq_var_time_min, inq_var_time_max, inq_var_time_mean, inq_var_time_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Total       : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            total_io_time_min, total_io_time_max, total_io_time_mean, total_io_time_var);
                    cur += num_written;
                }
            }
 
            MPI_Comm_free(&comm_io);
        }

//fprintf(stderr, "--------22222 timing result for group: %s : rank %d\n", gp_prof->adios_group_id->name, global_rank);
        unsigned long long my_length = cur - print_buf;
        unsigned long long my_offset = 0;
        MPI_Offset write_offset;
        MPI_Request request_hd;

        // calculate offset
        MPI_Scan(&my_length, &my_offset, 1, MPI_LONG_LONG, MPI_SUM, adios_mpi_comm_world_timing);
        write_offset = my_offset - my_length;

        profile_read.prof_file_name = (char *)calloc(strlen(gp_prof->file_name)+1+10, sizeof(char));    
        if(!profile_read.prof_file_name) {
            fprintf(stderr, "ADIOS Profiling: Cannot allocate memory\n");
            return -1;                
        } 
        strcat(profile_read.prof_file_name, gp_prof->file_name);
        strcat(profile_read.prof_file_name, ".prof.read");
   
        // zf2: for some strange reason MPI_File_open() fails with applicaiton codes written in C
        // error: ADIO_OPEN build_cb_config_list(674): No aggregators match
        // so here we use POSIX IO 
        if(print_flag) {
            int prof_file = open(profile_read.prof_file_name, 
                O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);  

            if(prof_file == -1) {
                fprintf (stderr, "ADIOS Profiling: Error opening file %s eror: %d\n",
                    profile_read.prof_file_name, errno); 
                return -1;
            }

            lseek(prof_file, write_offset, SEEK_SET); 
            ssize_t written = write(prof_file, print_buf, my_length);
            if(written != my_length) {
                fprintf (stderr, "ADIOS Profiling: Error opening file %s eror: %d\n",
                    profile_read.prof_file_name, errno);
                return -1;
            }
            close(prof_file);
        }
/*
    	// open log file using MPI IO API
        int err_class;
        rc = MPI_File_open(adios_mpi_comm_world_timing
                          ,profile_read.prof_file_name
                          ,MPI_MODE_WRONLY | MPI_MODE_APPEND | MPI_MODE_CREATE
                          ,NULL, &(profile_read.prof_file_ptr)
                          );

        if (rc != MPI_SUCCESS) {
            int error_class; 
            char err_string[200];
            int resultlen;
            MPI_Error_string(rc, err_string, &resultlen);
            fprintf (stderr, "ADIOS Profiling: Error opening file %s error %s \n",
               profile_read.prof_file_name, err_string);

            MPI_Error_class(rc, &error_class);
            fprintf (stderr, "ADIOS Profiling: Error opening file %s error %d \n", 
               profile_read.prof_file_name, error_class);
        }   

        // set file pointer to the right position
        rc = MPI_File_seek(profile_read.prof_file_ptr, write_offset, MPI_SEEK_SET);

        // write timing info to log file using explicit offset
        // the timing info will be ordered by global_rank
        MPI_Status status;
        if(print_flag) {
            rc = MPI_File_write(profile_read.prof_file_ptr
				                  ,print_buf
				                  ,my_length
				                  ,MPI_CHAR
				                  ,&status
				                  );

            if(rc != MPI_SUCCESS) {
                int error_class; 
                MPI_Error_class(rc, &error_class);
                fprintf(stderr, "ADIOS Profiling: Error %d returned by MPI_File_write() for group %s\n", 
                    error_class, gp_prof->file_name);
                return -1;
            }
        }

        // flush and close log file
        rc = MPI_File_close (&profile_read.prof_file_ptr);
        if(rc != MPI_SUCCESS) {
            int error_class; 
            MPI_Error_class(rc, &error_class);
            fprintf(stderr, "ADIOS Profiling: Error %d returned by MPI_File_close() for group %s\n", 
                error_class, gp_prof->file_name);
            return -1;
        }
        free(profile_read.prof_file_name);
*/
    }

    // cleanup
    for(i = 0; i < profile_read.num_groups; i ++) {
        free(profile_read.gp_prof_info[i].timing);    
        free(profile_read.gp_prof_info[i].file_name);    
    }
    free(profile_read.gp_prof_info);

    return 0;
}

int finalize_prof_read_all_()
{
    return finalize_prof_read_all();
}
