/*
 * Functions for process/thread to core pinning. The main purpose is to
 * provide an easy way to control the pinning through external parameters.
 *
 * TODO: integreate with hwloc for automatic topology discovery.
 */

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "thread_pin.h"

int smoky_neighbors[4][4] = {{0, 1, 2, 3},{4, 5, 6, 7},{8, 9, 10, 11},{12, 13, 14, 15}};
node_topology smoky_node_topo = {
    .num_numa_domains = 4,
    .num_cores = 16,
    .neighbors = NULL
};

// global variable to refer to the node topology
node_topology_t this_node_topo = NULL;

/*
 * discover the node toplogy. call this before any other functions in ths file
 */
node_topology_t get_node_topology()
{
    // hardcode the node topology here for smoky compute node
    smoky_node_topo.neighbors = smoky_neighbors;
    this_node_topo = &smoky_node_topo;
    return this_node_topo;
}

/*
 * in case gettid() is not implemented on system
 */
#include <sys/syscall.h>
pid_t gettid(void)
{
    pid_t tid = (pid_t) syscall(SYS_gettid);
    return tid;
}

/*
 * pin a thread to a specific core
 */
int pin_thread_to_core(pid_t tid, int core_id)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    int rc = sched_setaffinity(tid, sizeof(cpu_set_t), &cpuset);
    if(rc) {
        fprintf(stderr, "cannot set application thread affinity\n");
        perror("sched_setaffinity");
    }
    return rc;
}

/*
 * pin calling thread to the specified core
 */
int pin_thread_self_to_core(int core_id)
{
    return pin_thread_to_core(gettid(), core_id);
}

/*
 * print out the core ids to which the thread is pinned
 */
void print_thread_affinity(int mpi_rank, int omp_id, pid_t tid)
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int rc = sched_getaffinity(tid, sizeof(cpu_set_t), &cpuset);
    if(rc) {
        fprintf(stderr, "cannot get application thread affinity\n");
        perror("sched_getaffinity");
    }
    printf("MPI rank %d thread %d (tid=%d) is pinned to cores: ", mpi_rank, omp_id, tid);
    int i;
    for(i = 0; i < this_node_topo->num_cores; i ++) {
        if(CPU_ISSET(i, &cpuset)) {
            printf("%d ", i);
        }
    }
    printf("\n");
}   

void print_thread_self_affinity(int mpi_rank, int omp_id)
{
    print_thread_affinity(mpi_rank, omp_id, gettid());
}

void get_num_nodes(int *num_nodes) 
{
    char *temp_string = getenv("NUM_NODES");
    if(temp_string) {
        *num_nodes = atoi(temp_string);
    }
    else {
        fprintf(stderr, "environment variable NUM_NODES is not set.\n");
        *num_nodes = -1;
    }
}

/*
 * Fortran interface
 */
long long get_node_topology_()
{
    return (long long)get_node_topology();

}

int pin_thread_self_to_core_(int *core_id)
{
    return pin_thread_self_to_core(*core_id);
}

void print_thread_self_affinity_(int *mpi_rank, int *omp_id)
{
    print_thread_self_affinity(*mpi_rank, *omp_id);
}

void get_num_nodes_(int *num_nodes)
{
    get_num_nodes(num_nodes);
}
