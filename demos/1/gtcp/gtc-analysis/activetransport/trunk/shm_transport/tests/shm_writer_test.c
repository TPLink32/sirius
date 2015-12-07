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
#include "read_shm.h"

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
    // TODO: parameters
    char *fname = "ions-reduce1.bp";


    MPI_Comm comm = MPI_COMM_WORLD;

    // phase 1: initialization
    MPI_Barrier(comm);

    int rc = adios_read_shm_init(comm);


    // phase 2: read file
    
    // open
    shm_read_file_data *f = adios_read_shm_fopen(fname, comm, 1);
    if(!f) {
        fprintf(stderr, "cannot open file. %s:%d\n", __FILE__, __LINE__);
        return;
    }


    // keep reading new timesteps
    int time_index = 0;
    while(1) {
        // poll for next available timestep
        rc = adios_read_shm_get_timestep(f, time_index);       

        printf("file %s group %s timestep %d\n", f->file_name, f->group_name, f->timestep);

        shm_pg_info_t pgs = f->pgs;

        int i;
        for(i = 0; i < f->num_pgs; i ++) {
            printf("pg %d writer rank %d num vars %d\n", i, pgs[i].data_q->writer_rank);
            shm_var_info_t v = pgs[i].vars;

            int j;
            for(j = 0; j < pgs[i].num_vars; j ++) {
                printf("j=%d var %d name %s path %s ndims %d\n", j, v[j].name, v[j].path, v[j].ndims); 
            }
        }

        adios_read_shm_advance_timestep(f);
        time_index ++;
    }

    // close
    adios_read_shm_fclose(f);


    // phase 3: finalize
    MPI_Barrier(comm);

    adios_read_shm_finalize();

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
 
