/*
 * primitives for process coordination
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"

typedef struct _shm_coordinator {
    MPI_Comm comm;
    int rank;
    int size;
} shm_coordinator, *shm_coordinator_t;

shm_coordinator_t shm_create_coordinator(MPI_Comm comm)
{
    shm_coordinator_t c = (shm_coordinator_t) malloc(sizeof(shm_coordinator));
    if(!c) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }
    c->comm = comm;
    MPI_Comm_rank(comm, &c->rank);
    MPI_Comm_size(comm, &c->size);
    return c;
}

void shm_destroy_coordinator(shm_coordinator_t coordinator)
{
    free(coordinator);
}

int shm_bcast(shm_coordinator_t coordinator, int root, void *buf, int length)
{
    int k;
    MPI_Comm_rank(coordinator->comm, &k);
    int rc = MPI_Bcast(buf, length, MPI_BYTE, root, coordinator->comm);
}

int shm_coordinator_myid(shm_coordinator_t coordinator)
{
    return coordinator->rank;
}

int shm_coordinator_size(shm_coordinator_t coordinator)
{
    return coordinator->size;
}

int shm_barrier(shm_coordinator_t coordinator)
{
    return MPI_Barrier(coordinator->comm);
}

