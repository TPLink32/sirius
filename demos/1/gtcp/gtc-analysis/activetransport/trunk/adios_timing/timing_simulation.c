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

static simulation_timing_info simulation_profile;

extern MPI_Comm adios_mpi_comm_world_timing;
extern MPI_Comm adios_mpi_comm_self;

/*
 * record simulation start time (call it after MPI_Init)
 *
 */
void simulation_start()
{
    simulation_profile.sim_start_time = get_timestamp();
}

void simulation_start_()
{
    simulation_start();
}

/*
 * record simulation end time (call it before MPI_Finalize)
 *
 */
void simulation_end()
{
    simulation_profile.sim_end_time = get_timestamp();
}

void simulation_end_()
{
    simulation_end();  
}

/*
 * record simulation main loop start time 
 *
 */
void simulation_main_loop_start()
{
    simulation_profile.main_loop_start_time = get_timestamp();
}

void simulation_main_loop_start_()
{
    simulation_main_loop_start();
}

/*
 * record simulation main loop end time
 *
 */
void simulation_main_loop_end()
{
    simulation_profile.main_loop_end_time = get_timestamp();
}

void simulation_main_loop_end_()
{
    simulation_main_loop_end();
}

/*
 * record adios_init() call start time
 *
 */
void adios_init_start()
{
    simulation_profile.adios_init_start_time = get_timestamp();
}

void adios_init_start_()
{
    adios_init_start();
}

/*
 * record adios_init() call end time
 *
 */
void adios_init_end()
{
    simulation_profile.adios_init_end_time = get_timestamp();
}

void adios_init_end_()
{
    adios_init_end();
}

/*
 * record adios_finalize() call start time
 *
 */
void adios_finalize_start()
{
    simulation_profile.adios_finalize_start_time = get_timestamp();
}

void adios_finalize_start_()
{
    adios_finalize_start();
}

/*
 * record adios_finalize() call end time
 *
 */
void adios_finalize_end()
{
    simulation_profile.adios_finalize_end_time = get_timestamp();
}

void adios_finalize_end_()
{
    adios_finalize_end();
}

/*
 * record adios_read_init() call start time
 *
 */
void adios_read_init_start()
{
    simulation_profile.adios_read_init_start_time = get_timestamp();
}

void adios_read_init_start_()
{
    adios_read_init_start();
}

/*
 * record adios_read_init() call end time
 *
 */
void adios_read_init_end()
{
    simulation_profile.adios_read_init_end_time = get_timestamp();
}

void adios_read_init_end_()
{
    adios_read_init_end();
}

/*
 * record adios_read_finalize() call start time
 *
 */
void adios_read_finalize_start()
{
    simulation_profile.adios_read_finalize_start_time = get_timestamp();
}

void adios_read_finalize_start_()
{
    adios_read_finalize_start();
}

/*
 * record adios_read_finalize() call end time
 *
 */
void adios_read_finalize_end()
{
    simulation_profile.adios_read_finalize_end_time = get_timestamp();
}

void adios_read_finalize_end_()
{
    adios_read_finalize_end();
}

/*
 * initialize profiling 
 *
 */
int init_prof_simulation(char *prof_file_name
             ,int max_cycle_count
             ,int max_prof_point_count
             ,MPI_Comm comm
             )
{
    simulation_profile.communicator = comm;
    MPI_Comm_size(comm, &(simulation_profile.comm_size));
    MPI_Comm_rank(comm, &(simulation_profile.myid));
//    simulation_profile.comm_size = comm_size;
//    simulation_profile.myid = my_rank;
    simulation_profile.max_cycle_count = max_cycle_count;
    simulation_profile.cycle_count = 0;
    simulation_profile.cycle_timing = (cycle_timing_info *) malloc(sizeof(cycle_timing_info) * max_cycle_count);

    simulation_profile.max_prof_point_count = max_prof_point_count;
    simulation_profile.prof_point_count = 0;
    simulation_profile.prof_points = (prof_point_info *) malloc(sizeof(prof_point_info) * max_prof_point_count);
    int i;
    for(i = 0; i < max_prof_point_count; i ++) {
        simulation_profile.prof_points[i].cycles = (int *) malloc(sizeof(int) * max_cycle_count);
        simulation_profile.prof_points[i].start_time = (double *) malloc(sizeof(double) * max_cycle_count);
        simulation_profile.prof_points[i].end_time = (double *) malloc(sizeof(double) * max_cycle_count);
        simulation_profile.prof_points[i].time = (double *) malloc(sizeof(double) * max_cycle_count);
        simulation_profile.prof_points[i].counts = -1; // will point to current cycle
        simulation_profile.prof_points[i].tag = -1;
        simulation_profile.prof_points[i].current_cycle = -1;
    }
   
    if(!prof_file_name || !strcmp(prof_file_name, "")) {
        fprintf(stderr, "ADIOS Profiling: using file \"./log\" to save profiling results.\n");    
        simulation_profile.prof_file_name = "./log";          
    }
    else {
        simulation_profile.prof_file_name = prof_file_name;
    }

    return 0;
}

int init_prof_simulation_(char *prof_file_name
              ,int *max_cycle_count
              ,int *max_prof_point_count
              ,MPI_Fint *comm
              ,int *pfile_name_size
              )
{
    return init_prof_simulation(prof_file_name, *max_cycle_count, *max_prof_point_count, MPI_Comm_f2c(*comm));
}

/*
 * record start time of a simulation cycle
 *
 */
void cycle_start(int cycle)
{
    if(simulation_profile.cycle_count < simulation_profile.max_cycle_count) {
        simulation_profile.cycle_timing[simulation_profile.cycle_count].cycle = cycle;
        simulation_profile.cycle_timing[simulation_profile.cycle_count].cycle_start_time = get_timestamp();
    }
    else {
        fprintf(stderr, "ADIOS Profiling: cycle count exceeds max limit (%d)\n", simulation_profile.max_cycle_count);
        exit(-1);
    }
}

void cycle_start_(int *cycle)
{
    cycle_start(*cycle);
}

/*
 * record end time of a simulation cycle
 *
 */
void cycle_end(int cycle)
{
    simulation_profile.cycle_timing[simulation_profile.cycle_count].cycle = cycle;
    simulation_profile.cycle_timing[simulation_profile.cycle_count].cycle_end_time = get_timestamp();
    simulation_profile.cycle_timing[simulation_profile.cycle_count].cycle_time = simulation_profile.cycle_timing[simulation_profile.cycle_count].cycle_end_time 
                                              - simulation_profile.cycle_timing[simulation_profile.cycle_count].cycle_start_time;
    simulation_profile.cycle_count ++;
}

void cycle_end_(int *cycle)
{
    cycle_end(*cycle);
}


/*
 * define an arbitrary tracing  point and record start time
 *
 */
void prof_point_start(long long *prof_handle, int tag, int cycle)
{
    prof_point_info *pp = simulation_profile.prof_points;
    prof_point_info *p = NULL;

    int i;
    int maxi = simulation_profile.prof_point_count;
    for(i = 0; i < maxi; i ++) {
        if(tag == pp[i].tag) {
            p = &pp[i];
            break;
        }
    }

    if(!p) {
        // no match, initialize a new entry
        p = &pp[maxi];
        p->tag = tag;
        //p->current_cycle = cycle;
        simulation_profile.prof_point_count ++;
    }
    else if(p->counts > simulation_profile.max_cycle_count) {
        // TODO
        fprintf(stderr, "ADIOS Profiling: profiling point count (%d) exceeds max limit (%d)\n",
            p->counts, simulation_profile.max_prof_point_count);
        exit(-1);
    }
 
    if(p->current_cycle != cycle) { // first time in this cycle
        p->current_cycle = cycle;
        p->counts ++; 
        p->cycles[p->counts] = cycle;
        p->time[p->counts] = 0;
    }

    p->start_time[p->counts] = get_timestamp();
    *prof_handle = (long long)p;
//            fprintf(stderr, "ADIOS Profiling: im here (%d)\n",__LINE__);
}

void prof_point_start_(long long *gp_prof_handle, int *tag, int *cycle)
{
    prof_point_start(gp_prof_handle, *tag, *cycle);
}

/*
 * record end time of a profiling point
 *
 */
void prof_point_end(long long prof_handle, int tag, int cycle)
{
    prof_point_info *pp = (prof_point_info *)prof_handle; 
    if(!pp) {
        fprintf(stderr, "ADIOS Profiling: unkown profiling point (%d)\n", tag);
    }
    pp->end_time[pp->counts] = get_timestamp();
    // accumulate time since the profiling region may be entered multiple times  
    pp->time[pp->counts] += pp->end_time[pp->counts] - pp->start_time[pp->counts];
}

void prof_point_end_(long long *prof_handle, int *tag, int *cycle)
{
    prof_point_end(*prof_handle, *tag, *cycle);
}


/*
 * report profling results 
 *
 */
int finalize_prof_simulation()
{
    int i, j;
    int rc;

    if(simulation_profile.myid == 0) {
        simulation_profile.prof_file = fopen(simulation_profile.prof_file_name, "a+");
        if(!simulation_profile.prof_file) {
            return -1;
        } 
        fprintf(simulation_profile.prof_file, "\nTotal Simulation Timing results\n");
        fprintf(simulation_profile.prof_file, "Cycle No.         : \t %-18s \t %-18s \t %-18s \t %-18s\n",
            "min", "max", "mean", "var");
    }
    
    for(i = 0; i < simulation_profile.cycle_count; i ++) {
        // gather timing info to proc 0
        double cycle_time_min, cycle_time_max, cycle_time_mean, cycle_time_var;
        double cycle_stime_min, cycle_stime_max, cycle_stime_mean, cycle_stime_var;
        double cycle_etime_min, cycle_etime_max, cycle_etime_mean, cycle_etime_var;

        get_time_dist(simulation_profile.communicator
                      ,&simulation_profile.cycle_timing[i].cycle_time
                      ,&cycle_time_min
                      ,&cycle_time_max
                      ,&cycle_time_mean
                      ,&cycle_time_var
                      );

        get_time_dist(simulation_profile.communicator
                      ,&simulation_profile.cycle_timing[i].cycle_start_time
                      ,&cycle_stime_min
                      ,&cycle_stime_max
                      ,&cycle_stime_mean
                      ,&cycle_stime_var
                      );

        get_time_dist(simulation_profile.communicator
                      ,&simulation_profile.cycle_timing[i].cycle_end_time
                      ,&cycle_etime_min
                      ,&cycle_etime_max
                      ,&cycle_etime_mean
                      ,&cycle_etime_var
                      );

        if(simulation_profile.myid == 0) {
            fprintf(simulation_profile.prof_file, "cycle no \t %d\n", simulation_profile.cycle_timing[i].cycle);

            fprintf(simulation_profile.prof_file, "# cycle time  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                    cycle_time_min, cycle_time_max, cycle_time_mean, cycle_time_var);
            fprintf(simulation_profile.prof_file, "# cycle start : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                    cycle_stime_min, cycle_stime_max, cycle_stime_mean, cycle_stime_var);
            fprintf(simulation_profile.prof_file, "# cycle end   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                    cycle_etime_min, cycle_etime_max, cycle_etime_mean, cycle_etime_var);
        }
    }

    if(simulation_profile.myid == 0) {
        simulation_profile.prof_file = fopen(simulation_profile.prof_file_name, "a+");
        if(!simulation_profile.prof_file) {
            return -1;
        }
        fprintf(simulation_profile.prof_file, "\nProfiling Point Timing results\n");
        fprintf(simulation_profile.prof_file, "Cycle No.         : \t %-18s \t %-18s \t %-18s \t %-18s\n",
            "min", "max", "mean", "var");
    }

    for(j = 0; j < simulation_profile.prof_point_count; j ++) {
        prof_point_info *pp = &simulation_profile.prof_points[j];
        if(simulation_profile.myid == 0) {
            fprintf(simulation_profile.prof_file, "\ntag: \t %d\n", pp->tag);
        }
        for(i = 0; i <= pp->counts; i ++) {
            // gather timing info to proc 0
            double cycle_time_min, cycle_time_max, cycle_time_mean, cycle_time_var;
            double cycle_stime_min, cycle_stime_max, cycle_stime_mean, cycle_stime_var;
            double cycle_etime_min, cycle_etime_max, cycle_etime_mean, cycle_etime_var;
//            double cycle_time = pp->end_time[i] - pp->start_time[i]; 
            double cycle_time = pp->time[i];

            get_time_dist(simulation_profile.communicator
                          ,&cycle_time
                          ,&cycle_time_min
                          ,&cycle_time_max
                          ,&cycle_time_mean
                          ,&cycle_time_var
                          );

            get_time_dist(simulation_profile.communicator
                          ,&pp->start_time[i]
                          ,&cycle_stime_min
                          ,&cycle_stime_max
                          ,&cycle_stime_mean
                          ,&cycle_stime_var
                          );

            get_time_dist(simulation_profile.communicator
                          ,&pp->end_time[i]
                          ,&cycle_etime_min
                          ,&cycle_etime_max
                          ,&cycle_etime_mean
                          ,&cycle_etime_var
                          );

            if(simulation_profile.myid == 0) {
                fprintf(simulation_profile.prof_file, "cycle no \t %d\n", pp->cycles[i]);

                fprintf(simulation_profile.prof_file, "# total time  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                        cycle_time_min, cycle_time_max, cycle_time_mean, cycle_time_var);
                fprintf(simulation_profile.prof_file, "# start time  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                        cycle_stime_min, cycle_stime_max, cycle_stime_mean, cycle_stime_var);
                fprintf(simulation_profile.prof_file, "# end time    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                        cycle_etime_min, cycle_etime_max, cycle_etime_mean, cycle_etime_var);
            }
        }
    }

    // simulation timing
    double sim_time = simulation_profile.sim_end_time - simulation_profile.sim_start_time;
    double main_loop_time = simulation_profile.main_loop_end_time - simulation_profile.main_loop_start_time;
    double init_time = simulation_profile.main_loop_start_time - simulation_profile.sim_start_time;
    double finalize_time = simulation_profile.sim_end_time - simulation_profile.main_loop_end_time; 

    double sim_time_min, sim_time_max, sim_time_mean, sim_time_var;
    double sim_stime_min, sim_stime_max, sim_stime_mean, sim_stime_var;
    double sim_etime_min, sim_etime_max, sim_etime_mean, sim_etime_var;
    double main_loop_time_min, main_loop_time_max, main_loop_time_mean, main_loop_time_var;
    double main_loop_stime_min, main_loop_stime_max, main_loop_stime_mean, main_loop_stime_var;
    double main_loop_etime_min, main_loop_etime_max, main_loop_etime_mean, main_loop_etime_var;
    double init_time_min, init_time_max, init_time_mean, init_time_var;
    double finalize_time_min, finalize_time_max, finalize_time_mean, finalize_time_var;

    get_time_dist(simulation_profile.communicator
                  ,&sim_time
                  ,&sim_time_min
                  ,&sim_time_max
                  ,&sim_time_mean
                  ,&sim_time_var
                  );

    get_time_dist(simulation_profile.communicator
                  ,&simulation_profile.sim_start_time
                  ,&sim_stime_min
                  ,&sim_stime_max
                  ,&sim_stime_mean
                  ,&sim_stime_var
                  );

    get_time_dist(simulation_profile.communicator
                  ,&simulation_profile.sim_end_time
                  ,&sim_etime_min
                  ,&sim_etime_max
                  ,&sim_etime_mean
                  ,&sim_etime_var
                  );
    
    get_time_dist(simulation_profile.communicator
                  ,&main_loop_time
                  ,&main_loop_time_min
                  ,&main_loop_time_max
                  ,&main_loop_time_mean
                  ,&main_loop_time_var
                  );

    get_time_dist(simulation_profile.communicator
                  ,&simulation_profile.main_loop_start_time
                  ,&main_loop_stime_min
                  ,&main_loop_stime_max
                  ,&main_loop_stime_mean
                  ,&main_loop_stime_var
                  );

    get_time_dist(simulation_profile.communicator
                  ,&simulation_profile.main_loop_end_time
                  ,&main_loop_etime_min
                  ,&main_loop_etime_max
                  ,&main_loop_etime_mean
                  ,&main_loop_etime_var
                  );

    get_time_dist(simulation_profile.communicator
                  ,&init_time
                  ,&init_time_min
                  ,&init_time_max
                  ,&init_time_mean
                  ,&init_time_var
                  );

    get_time_dist(simulation_profile.communicator
                  ,&finalize_time
                  ,&finalize_time_min
                  ,&finalize_time_max
                  ,&finalize_time_mean
                  ,&finalize_time_var
                  );

    // adios_init and adios_finalize timing
    double adios_init_time = simulation_profile.adios_init_end_time - simulation_profile.adios_init_start_time;
    double adios_fin_time = simulation_profile.adios_finalize_end_time - simulation_profile.adios_finalize_start_time;

    double adios_init_time_min, adios_init_time_max, adios_init_time_mean, adios_init_time_var;
    double adios_init_stime_min, adios_init_stime_max, adios_init_stime_mean, adios_init_stime_var;
    double adios_init_etime_min, adios_init_etime_max, adios_init_etime_mean, adios_init_etime_var;
    double adios_fin_time_min, adios_fin_time_max, adios_fin_time_mean, adios_fin_time_var;
    double adios_fin_stime_min, adios_fin_stime_max, adios_fin_stime_mean, adios_fin_stime_var;
    double adios_fin_etime_min, adios_fin_etime_max, adios_fin_etime_mean, adios_fin_etime_var;

    get_time_dist(simulation_profile.communicator
                 ,&adios_init_time
                 ,&adios_init_time_min
                 ,&adios_init_time_max
                 ,&adios_init_time_mean
                 ,&adios_init_time_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_init_start_time)
                 ,&adios_init_stime_min
                 ,&adios_init_stime_max
                 ,&adios_init_stime_mean
                 ,&adios_init_stime_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_init_end_time)
                 ,&adios_init_etime_min
                 ,&adios_init_etime_max
                 ,&adios_init_etime_mean
                 ,&adios_init_etime_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&adios_fin_time
                 ,&adios_fin_time_min
                 ,&adios_fin_time_max
                 ,&adios_fin_time_mean
                 ,&adios_fin_time_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_finalize_start_time)
                 ,&adios_fin_stime_min
                 ,&adios_fin_stime_max
                 ,&adios_fin_stime_mean
                 ,&adios_fin_stime_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_finalize_end_time)
                 ,&adios_fin_etime_min
                 ,&adios_fin_etime_max
                 ,&adios_fin_etime_mean
                 ,&adios_fin_etime_var
                 );


    // adios_read_init and adios_read_finalize timing
    double adios_read_init_time = simulation_profile.adios_read_init_end_time - simulation_profile.adios_read_init_start_time;
    double adios_read_fin_time = simulation_profile.adios_read_finalize_end_time - simulation_profile.adios_read_finalize_start_time;

    double adios_read_init_time_min, adios_read_init_time_max, adios_read_init_time_mean, adios_read_init_time_var;
    double adios_read_init_stime_min, adios_read_init_stime_max, adios_read_init_stime_mean, adios_read_init_stime_var;
    double adios_read_init_etime_min, adios_read_init_etime_max, adios_read_init_etime_mean, adios_read_init_etime_var;
    double adios_read_fin_time_min, adios_read_fin_time_max, adios_read_fin_time_mean, adios_read_fin_time_var;
    double adios_read_fin_stime_min, adios_read_fin_stime_max, adios_read_fin_stime_mean, adios_read_fin_stime_var;
    double adios_read_fin_etime_min, adios_read_fin_etime_max, adios_read_fin_etime_mean, adios_read_fin_etime_var;

    get_time_dist(simulation_profile.communicator
                 ,&adios_read_init_time
                 ,&adios_read_init_time_min
                 ,&adios_read_init_time_max
                 ,&adios_read_init_time_mean
                 ,&adios_read_init_time_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_read_init_start_time)
                 ,&adios_read_init_stime_min
                 ,&adios_read_init_stime_max
                 ,&adios_read_init_stime_mean
                 ,&adios_read_init_stime_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_read_init_end_time)
                 ,&adios_read_init_etime_min
                 ,&adios_read_init_etime_max
                 ,&adios_read_init_etime_mean
                 ,&adios_read_init_etime_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&adios_read_fin_time
                 ,&adios_read_fin_time_min
                 ,&adios_read_fin_time_max
                 ,&adios_read_fin_time_mean
                 ,&adios_read_fin_time_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_read_finalize_start_time)
                 ,&adios_read_fin_stime_min
                 ,&adios_read_fin_stime_max
                 ,&adios_read_fin_stime_mean
                 ,&adios_read_fin_stime_var
                 );

    get_time_dist(simulation_profile.communicator
                 ,&(simulation_profile.adios_read_finalize_end_time)
                 ,&adios_read_fin_etime_min
                 ,&adios_read_fin_etime_max
                 ,&adios_read_fin_etime_mean
                 ,&adios_read_fin_etime_var
                 );

    if(simulation_profile.myid == 0) {
        fprintf(simulation_profile.prof_file, "\n\n# Simulation  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                sim_time_min, sim_time_max, sim_time_mean, sim_time_var);
        fprintf(simulation_profile.prof_file, "# Sim start   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                sim_stime_min, sim_stime_max, sim_stime_mean, sim_stime_var);
        fprintf(simulation_profile.prof_file, "# Sim end     : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                sim_etime_min, sim_etime_max, sim_etime_mean, sim_etime_var);

        fprintf(simulation_profile.prof_file, "\n# Main Loop   : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                main_loop_time_min, main_loop_time_max, main_loop_time_mean, main_loop_time_var);
        fprintf(simulation_profile.prof_file, "# main start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                main_loop_stime_min, main_loop_stime_max, main_loop_stime_mean, main_loop_stime_var);
        fprintf(simulation_profile.prof_file, "# main end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                main_loop_etime_min, main_loop_etime_max, main_loop_etime_mean, main_loop_etime_var);

        fprintf(simulation_profile.prof_file, "\n# Initialize  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                init_time_min, init_time_max, init_time_mean, init_time_var);

        fprintf(simulation_profile.prof_file, "\n# Finalize    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                finalize_time_min, finalize_time_max, finalize_time_mean, finalize_time_var);

        fprintf(simulation_profile.prof_file, "\n# adios_init  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_init_time_min, adios_init_time_max, adios_init_time_mean, adios_init_time_var);
        fprintf(simulation_profile.prof_file, "# adios_init start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_init_stime_min, adios_init_stime_max, adios_init_stime_mean, adios_init_stime_var);
        fprintf(simulation_profile.prof_file, "# adios_init end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_init_etime_min, adios_init_etime_max, adios_init_etime_mean, adios_init_etime_var);

        fprintf(simulation_profile.prof_file, "\n# adios_finalize  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_fin_time_min, adios_fin_time_max, adios_fin_time_mean, adios_fin_time_var);
        fprintf(simulation_profile.prof_file, "# adios_finalize start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_fin_stime_min, adios_fin_stime_max, adios_fin_stime_mean, adios_fin_stime_var);
        fprintf(simulation_profile.prof_file, "# adios_finalize end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_fin_etime_min, adios_fin_etime_max, adios_fin_etime_mean, adios_fin_etime_var);

        fprintf(simulation_profile.prof_file, "\n# adios_read_init  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_read_init_time_min, adios_read_init_time_max, adios_read_init_time_mean, adios_read_init_time_var);
        fprintf(simulation_profile.prof_file, "# adios_read_init start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_read_init_stime_min, adios_read_init_stime_max, adios_read_init_stime_mean, adios_read_init_stime_var);
        fprintf(simulation_profile.prof_file, "# adios_read_init end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_read_init_etime_min, adios_read_init_etime_max, adios_read_init_etime_mean, adios_read_init_etime_var);

        fprintf(simulation_profile.prof_file, "\n# adios_read_finalize  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_read_fin_time_min, adios_read_fin_time_max, adios_read_fin_time_mean, adios_read_fin_time_var);
        fprintf(simulation_profile.prof_file, "# adios_read_finalize start  : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_read_fin_stime_min, adios_read_fin_stime_max, adios_read_fin_stime_mean, adios_read_fin_stime_var);
        fprintf(simulation_profile.prof_file, "# adios_read_finalize end    : \t %-18f \t %-18f \t %-18f \t %-18f\n",
                adios_read_fin_etime_min, adios_read_fin_etime_max, adios_read_fin_etime_mean, adios_read_fin_etime_var);

        if(simulation_profile.prof_file) {
            rc = fflush(simulation_profile.prof_file);
            if(!rc) return rc;
            rc = fclose(simulation_profile.prof_file); 
            if(!rc) return rc;
        }
    }
    return 0;
}

int finalize_prof_simulation_()
{
    finalize_prof_simulation();
}


