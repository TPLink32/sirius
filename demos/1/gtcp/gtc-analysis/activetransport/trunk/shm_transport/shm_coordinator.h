#ifndef _SHM_COORDINATOR_H_
#define _SHM_COORDINATOR_H_
/*
 * primitives for process coordination
 *
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "mpi.h"

typedef struct _shm_coordinator {
    MPI_Comm comm; 
    int rank;
    int size;
} shm_coordinator, *shm_coordinator_t;

shm_coordinator_t shm_create_coordinator(MPI_Comm comm);

void shm_destroy_coordinator(shm_coordinator_t coordinator);

int shm_bcast(shm_coordinator_t coordinator, int root, char *buf, int length);

int shm_coordinator_myid(shm_coordinator_t coordinator);

int shm_coordinator_size(shm_coordinator_t coordinator);

int shm_barrier(shm_coordinator_t coordinator);

#ifdef __cplusplus
}
#endif

#endif

