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


  if(rank==0) {
      server();
      printf("Server done.\n");
  }
  else {
      sleep(5);
      client();
      printf("Client done.\n");
  }

  MPI_Finalize();
  return 0;
}

void server()
{
    char c;
    int shmid;
    char *shm, *s;

    key_t key = SHM_KEY;
    size_t size = SHM_SIZE;
    int shm_flag = SHM_FLAG;

    /*
     * server creates a shm region
     */
    shm_region_t region = shm_create_region(key, size, shm_flag);
    if(!region) {
        perror("shmget");
        printf("\n%d\n", errno);

#define PERR(type) if(errno == type) printf(#type);

        PERR(EACCES);
        PERR(EEXIST);
        PERR(EINVAL);
        PERR(ENFILE);
        PERR(ENOENT);
        PERR(ENOMEM);
        PERR(EIDRM);
        PERR(ENOSPC);
        PERR(EPERM);
        exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    shm = shm_attach_region(region, NULL, 0);
    if(!shm) {
        perror("shmat");
        exit(1);
    }

    /*
     * set memory allocator for a shared memory region
     */
    shm_malloc_t mm = shm_associate_allocator(region,
                                     &default_allocator,
                                     NULL 
                                    );

    /*
     * create a shm queue
     */
    shm_q_handle_t q = shm_q_create(1,
                                    NUM_SLOTS,  // # of slots in queuea
                                    mm,
                                    128,        // start offset
                                    NUM_SLOTS*CHUNK_SIZE,     // TODO
                                    1,          // static
                                    CHUNK_SIZE  // # of bytes per chunk
                                   );


    /*
     * Now put some things into the beginnging memory for the
     * other process to read.
     */
    s = shm;

    for (c = 'a'; c <= 'z'; c++)
        *s++ = c;
    *s = 0;

    /*
     * Now tell client the shared memory and queue is ready
     */
    MPI_Barrier(MPI_COMM_WORLD);

    /*
     * keep sending data through the queue
     */
    int i;
    char output_data[CHUNK_SIZE];
    for(i = 0; i < NUM_CHUNKS; i ++) {
 
        printf("now put %d in queue\n", i);

        // wait for current slot to be empty
        while(!shm_q_is_empty(q));

        memset(output_data, i%26+'a', CHUNK_SIZE);
        output_data[CHUNK_SIZE-1] = '\0'; 

        // put new data in
        int rc = shm_q_put(q, output_data, CHUNK_SIZE);
    }   

    /*
     * Finally, we wait until the other process 
     * changes the first character of our memory
     * to '*', indicating that it has read what 
     * we put there.
     */
    while (*shm != '*')
        sleep(1);

    shm_q_destroy(q);
    shm_delete_allocator(mm);
    shm_delete_region(region);
}




void client()
{
    char *shm, *s;

    /*
     * We need to get the segment named
     * "5678", created by the server.
     */
    key_t key = SHM_KEY;
    size_t size = SHM_SIZE;
    int shm_flag = SHM_FLAG;

    /*
     * Now wait for the shared memory and queue to be ready
     */
    MPI_Barrier(MPI_COMM_WORLD);

    /*
     * Locate the segment.
     */
    shm_region_t region = shm_get_region(key, size, shm_flag);
    if(!region) {
        perror("shmget");
        exit(1);
    }

    /*
     * Now we attach the segment to our data space.
     */
    shm = shm_attach_region(region, NULL, 0);
    if(!shm) {
        perror("shmat");
        exit(1);
    }

    /*
     * create a shm queue
     */

    /*
     * set memory allocator for a shared memory region
     */
    shm_malloc_t mm = shm_associate_allocator(region,
                                     &default_allocator,
                                     NULL
                                    );

    shm_q_handle_t q = shm_q_create(0,
                                    NUM_SLOTS,  // # of slots in queuea
                                    mm,
                                    128,        // start offset
                                    NUM_SLOTS*CHUNK_SIZE,     // TODO
                                    1,          // static
                                    CHUNK_SIZE  // # of bytes per chunk
                                   );

    /*
     * Now read what the server put in the beginning memory.
     */
    for(s = shm; *s != 0; s++)
        putchar(*s);
    putchar('\n');

    /*
     * now read chunks from queue
     */
    int i;
    for(i = 0; i < NUM_CHUNKS; i ++) {
        // wait for current slot to be full
        while(!shm_q_is_full(q));

        shm_q_slot_t slot = shm_q_get_current_slot(q);

        char *data = (char *)(slot->data + q->mm->region->base_addr);
        printf("now read %d from queue\n", i);
        printf("%s\n", data);
     
        shm_q_release_current_slot(q);
    }
    

    /*
     * Finally, change the first character of the 
     * segment to '*', indicating we have read 
     * the segment.
     */
    *shm = '*';

    shm_q_destroy(q);
    shm_delete_allocator(mm);
}
 
