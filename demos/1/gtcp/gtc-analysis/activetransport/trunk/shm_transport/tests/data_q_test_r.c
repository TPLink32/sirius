#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "shm_mm.h"
#include "shm_malloc.h"
#include "shm_queue.h"
#include "meta_data.h"

#define SHMSZ 1024
#define NUM_CHUNKS 1000
#define NUM_SLOTS  5
#define CHUNK_SIZE 100
#define SHM_KEY 5678
#define SHM_SIZE (100*1024*1024)
#define SHM_FLAG 0666

void server();
void client();

int main (argc, argv)
     int argc;
     char *argv[];
{
  int rank, size;

  MPI_Init (&argc, &argv);	/* starts MPI */
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);	/* get current process id */
  MPI_Comm_size (MPI_COMM_WORLD, &size);	/* get number of processes */
  printf("Hello world from process %d of %d\n", rank, size);

  printf("sizeof key_t=%d size_t=%d\n", sizeof(key_t), sizeof(size_t));


  server();
  printf("Server done.\n");
 
  MPI_Finalize();
  return 0;
}

void server()
{

    int total_size = 100*1024;
    int max_writers = 4;

fprintf(stderr, "im here %s:%d\n", __FILE__, __LINE__);
    // server create meta data region
    int rc = create_meta_region(total_size, max_writers);
    if(rc) {
        fprintf(stderr, "cannot create meta data region %d\n", rc);
        return;
    }
fprintf(stderr, "im here %s:%d\n", __FILE__, __LINE__);

    /*
     * Now tell client the shared memory and queue is ready
     */
    MPI_Barrier(MPI_COMM_WORLD);

    // do nothing, clients are attaching meta data region

    // phase 2: create data queues
    MPI_Barrier(MPI_COMM_WORLD);
     
    // read from data queue
    sleep(10); 

    // phase 3: finalize
    MPI_Barrier(MPI_COMM_WORLD);
fprintf(stderr, "im here %s:%d\n", __FILE__, __LINE__);
    // destroy the meta data region
    rc = destroy_meta_region();
fprintf(stderr, "im here %s:%d\n", __FILE__, __LINE__);
}


extern shm_meta_region_t meta_region;

void client()
{
    int total_size = 100*1024;

    /*
     * Now wait for the shared memory and queue to be ready
     */
    MPI_Barrier(MPI_COMM_WORLD);

    // client get meta data region created by server
    int rc = initialize_meta_region(total_size);
    if(rc) {
        fprintf(stderr, "cannot get meta data region %d\n", rc);
        return;
    }
 
    printf("meta data region: total size=%d entry size=%d next=%lu\n", 
        meta_region->meta_data->total_size,
        meta_region->meta_data->entry_size,
        meta_region->meta_data->next
    );

    // phase 2: create data queue
    MPI_Barrier(MPI_COMM_WORLD);

    int mpi_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    shm_data_queue_t data_q = create_data_queue("restart.bp",
                                   "restart",
                                   mpi_rank,
                                   101*1024*1024, // total data queue size
                                   1, // is static
                                   1, // number of slot
                                   100*1024*1024 // chunk size
                                  );
fprintf(stderr, "im here %s:%d\n", __FILE__, __LINE__);

    rc = delete_data_queue(data_q);

fprintf(stderr, "im here %d %s:%d\n", rc, __FILE__, __LINE__);


    // phase 3: finalize
    // detach the meta data region    
    rc = finalize_meta_region();
    MPI_Barrier(MPI_COMM_WORLD);
}
 
