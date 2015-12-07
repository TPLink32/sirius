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

static io_prof_setting_write profile_write;

extern MPI_Comm adios_mpi_comm_world_timing;
extern MPI_Comm adios_mpi_comm_self;

/*
 * find profiling struct by group id for writing
 */
group_prof_info_write *get_prof_by_group_name_write(char *group_name)
{
    int i = 0;
    for(; i < profile_write.num_groups; i ++) {
        if(!strcmp(profile_write.gp_prof_info[i].adios_group_id->name, group_name)) {
            return &profile_write.gp_prof_info[i];
        }
    }

    return NULL;
}

/*
 * get communicator handle for specified group
 */
MPI_Comm get_comm_by_group(struct adios_group_struct *group)
{
    if (group->group_comm) {
        struct adios_var_struct *var = adios_find_var_by_name(group->vars, group->group_comm, group->all_unique_var_names);

        if (var) {
            if (var->data) {
                return *(MPI_Comm *)var->data;
            }
            else {
//                fprintf (stderr, "ADIOS Profiling: coordination-communicator: %s not provided. "
//                                 "Using adios_mpi_comm_world instead\n"
//                        ,group->group_comm
//                        );
                return adios_mpi_comm_world_timing;
            }
        }
        else {
//            fprintf (stderr, "ADIOS Profiling: coordination-communicator: %s not found in "
//                             "adios-group.  Using adios_mpi_comm_world instead\n"
//                    ,group->group_comm
//                    );

            return adios_mpi_comm_world_timing;
        }
    }
    else
    {
        return MPI_COMM_NULL;
    }
}

/*
 * initialize profiling for writing
 *
 * used inside ADIOS so we can record io timing info for each every adios group
 */
int init_prof_internal_write(struct adios_group_list_struct *group_list)
{
    profile_write.prof_file_name = NULL;          
    profile_write.num_groups = 0;
    struct adios_group_list_struct *gp_ptr = group_list;  
    while(gp_ptr) {
        profile_write.num_groups ++;
        gp_ptr = gp_ptr->next;
    }

    profile_write.gp_prof_info = (group_prof_info_write *) malloc(profile_write.num_groups * sizeof(group_prof_info_write));

    gp_ptr = group_list;  
    int i = 0;
    while(i < profile_write.num_groups) {
        profile_write.gp_prof_info[i].adios_group_id = gp_ptr->group;
        profile_write.gp_prof_info[i].communicator = MPI_COMM_NULL;
        profile_write.gp_prof_info[i].comm_size = 0;
        profile_write.gp_prof_info[i].myid = 0;

        // TODO: hardcode max_io_count now; later change to use link list or set by env variable
        profile_write.gp_prof_info[i].max_io_count = MAX_IO_COUNT; 
        profile_write.gp_prof_info[i].io_count = 0;

        // NOTE: 
        // timing_info is initialized with all bits set to 0. 
        // Therefore, timing[i].open_start_time == 0 means this process didn't do IO at that cycle
        profile_write.gp_prof_info[i].timing = (timing_info_write *) calloc(profile_write.gp_prof_info[i].max_io_count, sizeof(timing_info_write));         

        gp_ptr = gp_ptr->next;
        i ++;
    }

    return 0;
}

/*
 * initialize profiling for writing
 *
 */
int init_prof_write_all() 
{
    struct adios_group_list_struct *g = adios_get_groups();
    init_prof_internal_write(g);
}

int init_prof_write_all_() 
{
    init_prof_write_all();
}

/*
 * record open start time for specified group
 *
 */
void open_start_for_group(long long *gp_prof_handle, char *group_name, int cycle)
{
//int rank;
//MPI_Comm_rank(adios_mpi_comm_world, &rank);
//fprintf(stderr, "------------ADIOS Profiling: open start group: %s rank: %d\n", group_name, rank);

    group_prof_info_write *gp_prof = get_prof_by_group_name_write(group_name);

    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group \"%s\"\n", group_name);
    }

    *gp_prof_handle = (long long)gp_prof;

    if(gp_prof->io_count < gp_prof->max_io_count) {
        gp_prof->timing[gp_prof->io_count].cycle = cycle;
        gp_prof->timing[gp_prof->io_count].open_start_time = get_timestamp();
    }
    else {
        fprintf(stderr, "ADIOS Profiling: io count exceeds max limit (%d)\n", gp_prof->max_io_count);
        exit(-1);
    }
//fprintf(stderr, "------------ADIOS Profiling: open start end group: %s rank: %d\n", group_name, rank);
}

void open_start_for_group_(long long *gp_prof_handle, char *group_name, int *cycle, int *gp_name_size)
{
    open_start_for_group(gp_prof_handle, group_name, *cycle);
}

/*
 * record open end time for specified group
 *
 */
void open_end_for_group(long long gp_prof_handle, int cycle)
{
//int rank;
//MPI_Comm_rank(adios_mpi_comm_world, &rank);
//fprintf(stderr, "------------ADIOS Profiling: open end group: rank: %d\n", rank);

    group_prof_info_write *gp_prof = (group_prof_info_write *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].open_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].open_time = gp_prof->timing[gp_prof->io_count].open_end_time 
                                              - gp_prof->timing[gp_prof->io_count].open_start_time;
}

void open_end_for_group_(long long *gp_prof_handle, int *cycle)
{
    open_end_for_group(*gp_prof_handle, *cycle);
}

/*
 * record group_size start time for specified group
 *
 */
void group_size_start_for_group(long long gp_prof_handle, int cycle)
{
    group_prof_info_write *gp_prof = (group_prof_info_write *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].grp_size_start_time = get_timestamp();
}

void group_size_start_for_group_(long long *gp_prof_handle, int *cycle)
{
    group_size_start_for_group(*gp_prof_handle, *cycle);
}

/*
 * record group_size end time for specified group
 *
 */
void group_size_end_for_group(long long gp_prof_handle, int cycle)
{
    group_prof_info_write *gp_prof = (group_prof_info_write *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].grp_size_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].grp_size_time = gp_prof->timing[gp_prof->io_count].grp_size_end_time
                                              - gp_prof->timing[gp_prof->io_count].grp_size_start_time;

}

void group_size_end_for_group_(long long *gp_prof_handle, int *cycle)
{
    group_size_end_for_group(*gp_prof_handle, *cycle);
}
/*
 * record write start time for specified group
 *
 */
void write_start_for_group(long long gp_prof_handle, int cycle)
{
//int rank;
//MPI_Comm_rank(adios_mpi_comm_world, &rank);
//fprintf(stderr, "------------ADIOS Profiling: write start group: rank: %d\n", rank);

    group_prof_info_write *gp_prof = (group_prof_info_write *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].write_start_time = get_timestamp();
}

void write_start_for_group_(long long *gp_prof_handle, int *cycle)
{
    write_start_for_group(*gp_prof_handle, *cycle);
}

/*
 * record write end time for specified group
 *
 */
void write_end_for_group(long long gp_prof_handle, int cycle)
{
//int rank;
//MPI_Comm_rank(adios_mpi_comm_world, &rank);
//fprintf(stderr, "------------ADIOS Profiling: write end group: rank: %d\n", rank);

    group_prof_info_write *gp_prof = (group_prof_info_write *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].write_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].write_time = gp_prof->timing[gp_prof->io_count].write_end_time 
                                              - gp_prof->timing[gp_prof->io_count].write_start_time;
}

void write_end_for_group_(long long *gp_prof_handle, int *cycle)
{
    write_end_for_group(*gp_prof_handle, *cycle);
}

/*
 * record close start time for specified group
 *
 */
void close_start_for_group(long long gp_prof_handle, int cycle)
{
//int rank;
//MPI_Comm_rank(adios_mpi_comm_world, &rank);
//fprintf(stderr, "------------ADIOS Profiling: close start group: rank: %d\n", rank);


    group_prof_info_write *gp_prof = (group_prof_info_write *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    // Right now is the only chance to get group's communicator
    // because at the end of adios_close all elements in adios_var struct
    // will get cleaned
    gp_prof->communicator = get_comm_by_group(gp_prof->adios_group_id);

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].close_start_time = get_timestamp();
}

void close_start_for_group_(long long *gp_prof_handle, int *cycle)
{
    close_start_for_group(*gp_prof_handle, *cycle);
}

/*
 * record close end time for specified group
 *
 */
void close_end_for_group(long long gp_prof_handle, int cycle)
{
//int rank;
//MPI_Comm_rank(adios_mpi_comm_world, &rank);


    group_prof_info_write *gp_prof = (group_prof_info_write *)gp_prof_handle;
    if(!gp_prof) {
        fprintf(stderr, "ADIOS Profiling: unkown group\n");
    }

    gp_prof->timing[gp_prof->io_count].cycle = cycle;
    gp_prof->timing[gp_prof->io_count].close_end_time = get_timestamp();
    gp_prof->timing[gp_prof->io_count].close_time = gp_prof->timing[gp_prof->io_count].close_end_time 
                                              - gp_prof->timing[gp_prof->io_count].close_start_time;
    gp_prof->io_count ++;
}

void close_end_for_group_(long long *gp_prof_handle, int *cycle)
{
    close_end_for_group(*gp_prof_handle, *cycle);
}

/*
 * Report timing info for all groups
 *
 */
int finalize_prof_write_all()
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

    for(g = 0; g < profile_write.num_groups; g ++) {
        group_prof_info_write *gp_prof = &profile_write.gp_prof_info[g];

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
            num_written = sprintf(cur, "Timing results for group: %s\n", gp_prof->adios_group_id->name);
            cur += num_written;
            num_written = sprintf(cur, "Operations    : \t %-18s \t %-18s \t %-18s \t %-18s\n",
                "min", "max", "mean", "var");
            cur += num_written;
        }
//fprintf(stderr, "--------11111 timing result for group: %s : rank %d\n", gp_prof->adios_group_id->name, global_rank);

        for(i = 0; i < gp_prof->io_count; i ++) {
            // gather timing info to proc 0
            double open_time_min, open_time_max, open_time_mean, open_time_var;
            double open_stime_min, open_stime_max, open_stime_mean, open_stime_var;
            double open_etime_min, open_etime_max, open_etime_mean, open_etime_var;
            double grp_size_time_min, grp_size_time_max, grp_size_time_mean, grp_size_time_var;
            double grp_size_stime_min, grp_size_stime_max, grp_size_stime_mean, grp_size_stime_var;
            double grp_size_etime_min, grp_size_etime_max, grp_size_etime_mean, grp_size_etime_var;
            double write_time_min, write_time_max, write_time_mean, write_time_var;
            double write_stime_min, write_stime_max, write_stime_mean, write_stime_var;
            double write_etime_min, write_etime_max, write_etime_mean, write_etime_var;
            double close_time_min, close_time_max, close_time_mean, close_time_var;
            double close_stime_min, close_stime_max, close_stime_mean, close_stime_var;
            double close_etime_min, close_etime_max, close_etime_mean, close_etime_var;
            double total_io_time, total_io_time_min, total_io_time_max, total_io_time_mean, total_io_time_var;

//added by Jianting
// zf2: we will seperate read and write timings
/*
	    double fopen_time_min, fopen_time_max, fopen_time_mean, fopen_time_var;
	    double fopen_stime_min, fopen_stime_max, fopen_stime_mean, fopen_stime_var;
	    double fopen_etime_min, fopen_etime_max, fopen_etime_mean, fopen_etime_var;
	    double read_time_min, read_time_max, read_time_mean, read_time_var;
	    double read_stime_min, read_stime_max, read_stime_mean, read_stime_var;
	    double read_etime_min, read_etime_max, read_etime_mean, read_etime_var;
	    double fclose_time_min, fclose_time_max, fclose_time_mean, rclose_time_var;
	    double fclose_stime_min, fclose_stime_max, flcose_stime_mean, fclose_stime_var;
	    double flcose_etime_min, fclose_etime_max, fclose_etime_mean, fclose_etime_var;
	    double gopen_time_min, gopen_time_max, gopen_time_mean, gopen_time_var;
	    double gopen_stime_min, gopen_stime_max, gopen_stime_mean, gopen_stime_var;
	    double gopen_etime_min, gopen_etime_max, gopen_etime_mean, gopen_etime_var;
	    double gclose_time_min, gclose_time_max, gclose_time_mean, gclose_time_var;
	    double gclose_stime_min, gclose_stime_max, gclose_stime_mean, gclose_stime_var;
	    double gclose_etime_min, gclose_etime_max, glcose_etime_mean, gclose_etime_var;
*/

            // since there may be only part of processes within the comm which actually did IO,
            // we need to first check how many processes did IO
            int did_io = (gp_prof->timing[i].open_start_time != 0)? 1 : 0;

            MPI_Comm comm_io;
            rc = MPI_Comm_split(gp_prof->communicator, did_io, gp_prof->myid, &comm_io);
            if(rc != MPI_SUCCESS) {
                int error_class;
                MPI_Error_class(rc, &error_class);
                fprintf(stderr, "ADIOS Profiling: Error %d returned by MPI_Comm_split() for group %s\n",
                    error_class, gp_prof->adios_group_id->name);
                return -1;
            }

            int comm_io_rank, comm_io_size;
            MPI_Comm_rank(comm_io, &comm_io_rank);
            MPI_Comm_size(comm_io, &comm_io_size);
            
            if(did_io) {
                get_time_dist(comm_io
                              ,&gp_prof->timing[i].open_time
                              ,&open_time_min
                              ,&open_time_max
                              ,&open_time_mean
                              ,&open_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].open_start_time
                              ,&open_stime_min
                              ,&open_stime_max
                              ,&open_stime_mean
                              ,&open_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].open_end_time
                              ,&open_etime_min
                              ,&open_etime_max
                              ,&open_etime_mean
                              ,&open_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].grp_size_time
                              ,&grp_size_time_min
                              ,&grp_size_time_max
                              ,&grp_size_time_mean
                              ,&grp_size_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].grp_size_start_time
                              ,&grp_size_stime_min
                              ,&grp_size_stime_max
                              ,&grp_size_stime_mean
                              ,&grp_size_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].grp_size_end_time
                              ,&grp_size_etime_min
                              ,&grp_size_etime_max
                              ,&grp_size_etime_mean
                              ,&grp_size_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].write_time
                              ,&write_time_min
                              ,&write_time_max
                              ,&write_time_mean
                              ,&write_time_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].write_start_time
                              ,&write_stime_min
                              ,&write_stime_max
                              ,&write_stime_mean
                              ,&write_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].write_end_time
                              ,&write_etime_min
                              ,&write_etime_max
                              ,&write_etime_mean
                              ,&write_etime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].close_time
                              ,&close_time_min
                              ,&close_time_max
                              ,&close_time_mean
                              ,&close_time_var
                              );    

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].close_start_time
                              ,&close_stime_min
                              ,&close_stime_max
                              ,&close_stime_mean
                              ,&close_stime_var
                              );

                get_time_dist(comm_io
                              ,&gp_prof->timing[i].close_end_time
                              ,&close_etime_min
                              ,&close_etime_max
                              ,&close_etime_mean
                              ,&close_etime_var
                              );

                total_io_time = gp_prof->timing[i].close_end_time - gp_prof->timing[i].open_start_time; 

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

                    num_written = sprintf(cur, "# Open        : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            open_time_min, open_time_max, open_time_mean, open_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Open start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            open_stime_min, open_stime_max, open_stime_mean, open_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Open end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            open_etime_min, open_etime_max, open_etime_mean, open_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Group_size  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            grp_size_time_min, grp_size_time_max, grp_size_time_mean, grp_size_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# group_size start : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            grp_size_stime_min, grp_size_stime_max, grp_size_stime_mean, grp_size_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# group_size end   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            grp_size_etime_min, grp_size_etime_max, grp_size_etime_mean, grp_size_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Write       : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            write_time_min, write_time_max, write_time_mean, write_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Write start : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            write_stime_min, write_stime_max, write_stime_mean, write_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Write end   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            write_etime_min, write_etime_max, write_etime_mean, write_etime_var);
                    cur += num_written;

                    num_written = sprintf(cur, "# Close       : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            close_time_min, close_time_max, close_time_mean, close_time_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Close start : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            close_stime_min, close_stime_max, close_stime_mean, close_stime_var);
                    cur += num_written;
                    num_written = sprintf(cur, "# Close end   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                            close_etime_min, close_etime_max, close_etime_mean, close_etime_var);
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

        profile_write.prof_file_name = (char *)calloc(strlen(gp_prof->adios_group_id->name)+1+11, sizeof(char));    
        if(!profile_write.prof_file_name) {
            fprintf(stderr, "ADIOS Profiling: Cannot allocate memory\n");
            return -1;                
        } 
        strcat(profile_write.prof_file_name, gp_prof->adios_group_id->name);
        strcat(profile_write.prof_file_name, ".prof.write");

    	// open log file using MPI IO API
        int err_class;
        rc = MPI_File_open(adios_mpi_comm_world_timing
                          ,profile_write.prof_file_name
                          ,MPI_MODE_WRONLY | MPI_MODE_APPEND | MPI_MODE_CREATE
                          ,MPI_INFO_NULL, &profile_write.prof_file_ptr);

        if (rc != MPI_SUCCESS) {
            fprintf (stderr, "ADIOS Profiling: Error opening file %s\n", profile_write.prof_file_name);
        }   

        // set file pointer to the right position
        rc = MPI_File_seek(profile_write.prof_file_ptr, write_offset, MPI_SEEK_SET);

//fprintf(stderr, "--------33333 timing result for group: %s : rank %d\n", gp_prof->adios_group_id->name, global_rank);
        // write timing info to log file using explicit offset
        // the timing info will be ordered by global_rank
        MPI_Status status;
        if(print_flag) {
            rc = MPI_File_write(profile_write.prof_file_ptr
				                  ,print_buf
				                  ,my_length
				                  ,MPI_CHAR
				                  ,&status
				                  );

            if(rc != MPI_SUCCESS) {
                int error_class; 
                MPI_Error_class(rc, &error_class);
                fprintf(stderr, "ADIOS Profiling: Error %d returned by MPI_File_write() for group %s\n", 
                    error_class, gp_prof->adios_group_id->name);
                return -1;
            }
        }

//fprintf(stderr, "--------44444 timing result for group: %s : rank %d\n", gp_prof->adios_group_id->name, global_rank);
        // flush and close log file
        rc = MPI_File_close (&profile_write.prof_file_ptr);
        if(rc != MPI_SUCCESS) {
            int error_class; 
            MPI_Error_class(rc, &error_class);
            fprintf(stderr, "ADIOS Profiling: Error %d returned by MPI_File_close() for group %s\n", 
                error_class, gp_prof->adios_group_id->name);
            return -1;
        }
        free(profile_write.prof_file_name);
//fprintf(stderr, "--------55555 timing result for group: %s : rank %d\n", gp_prof->adios_group_id->name, global_rank);
    }

    // cleanup
    for(i = 0; i < profile_write.num_groups; i ++) {
        free(profile_write.gp_prof_info[i].timing);    
    }
    free(profile_write.gp_prof_info);

    return 0;
}


int finalize_prof_write_all_()
{
    return finalize_prof_write_all();
}

