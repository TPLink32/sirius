#ifndef _THREAD_PIN_H_
#define _THREAD_PIN_H_
/*
 * Functions for process/thread to core pinning. The main purpose is to
 * provide an easy way to control the pinning through external parameters.
 *
 * TODO: integreate with hwloc for automatic topology discovery.
 */
#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>

// TODO: move all parameters into a header file

// TODO: cache topology
typedef struct _node_topology {
    int num_numa_domains;
    int num_cores;
    int **neighbors; // 2d array: num_numa_domains x num_cores
} node_topology, *node_topology_t;

/*
 * discover the node toplogy. call this before any other functions in ths file
 */
node_topology_t get_node_topology();

/*
 * Note: gettid() call is not available on smoky so we provide our own
 */
pid_t gettid(void);

/*
 * pin a thread to a specific core
 */
int pin_thread_to_core(pid_t tid, int core_id);

/*
 * pin calling thread to the specified core
 */
int pin_thread_self_to_core(int core_id);

/*
 * print out the core ids to which the thread is pinned
 */
void print_thread_affinity(int mpi_rank, int omp_id, pid_t tid);
void print_thread_self_affinity(int mpi_rank, int omp_id);

/*
 * get number of nodes the calling mpi program is running on
 */
void get_num_nodes(int *num_nodes);

/*
 * Fortran interface
 */
long long get_node_topology_();
int pin_thread_self_to_core_(int *core_id);
void print_thread_self_affinity_(int *mpi_rank, int *omp_id);
void get_num_nodes_(int *num_nodes);

#ifdef __cplusplus
}
#endif

#endif

