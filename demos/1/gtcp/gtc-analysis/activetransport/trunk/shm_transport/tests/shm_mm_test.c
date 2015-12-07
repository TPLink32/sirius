#include <stdio.h>
#include <mpi.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include "shm_mm.h"

#define SHMSZ 1024


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
  printf( "Hello world from process %d of %d\n", rank, size );

  printf("sizeof key_t=%d size_t=%d\n", sizeof(key_t), sizeof(size_t));


  if(rank==0) {
      server();
  }
  else {
      sleep(5);
      client();
  }

  MPI_Finalize();
  return 0;
}



#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <errno.h>

#define SHMSZ 1024

void server()
{
    char c;
    int shmid;
    char *shm, *s;

    key_t key = 5678;
    size_t size = 400*1024*1024;
    int shm_flag = 0666;

    /*
     * server creates a shm region
     */
    shm_region_t region = shm_create_region(key, size, shm_flag);
    if(!region) {
        perror("shmget");
        printf("11111error no \n%d\n", errno);

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
     * Now put some things into the memory for the
     * other process to read.
     */
    s = shm;

    for (c = 'a'; c <= 'z'; c++)
        *s++ = c;
    *s = 0;

    MPI_Barrier(MPI_COMM_WORLD);

    /*
     * Finally, we wait until the other process 
     * changes the first character of our memory
     * to '*', indicating that it has read what 
     * we put there.
     */
    while (*shm != '*')
        sleep(1);
    shm_delete_region(region);
}




void client()
{
    char *shm, *s;

    /*
     * We need to get the segment named
     * "5678", created by the server.
     */
    key_t key = 5678;
    size_t size = 400*1024*1024;
    int shm_flag = 0666;

    MPI_Barrier(MPI_COMM_WORLD);
    /*
     * Locate the segment.
     */
    shm_region_t region = shm_create_region(key, size, shm_flag);
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
     * Now read what the server put in the memory.
     */
    for(s = shm; *s != 0; s++)
        putchar(*s);
    putchar('\n');

    /*
     * Finally, change the first character of the 
     * segment to '*', indicating we have read 
     * the segment.
     */
    *shm = '*';

    shm_dettach_region(region);
}

