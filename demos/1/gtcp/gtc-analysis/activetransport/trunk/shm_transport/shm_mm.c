/*
 * Low level shared memory management functions.
 *
 * It is implemented using System V shared memory APIs since it the only one
 * available on Compute Node Linux.
 *   
 */
#include <stdio.h> 
#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "shm_mm.h"

/*
 * create a shared memory region. 
 */ 
shm_region_t shm_create_region(key_t key, size_t size, int shm_flag)
{
    shm_region_t region = (shm_region_t) malloc(sizeof(shm_region));
    if(!region) {
        fprintf(stderr, "cannot allocate memory. %s%d\n", __FILE__, __LINE__);
        return NULL;
    }
    memset(region, 0, sizeof(shm_region));
    
    if((region->id = shmget(key, size, IPC_CREAT | shm_flag)) < 0) {
        perror("shmget");
        fprintf(stderr, "error no: %d\n", errno);
        free(region);
        return NULL;
    }        
    region->key = key;
    region->size = size;
    region->shm_flag = shm_flag;
    return region;
}
 
/*
 * attach to a created shared memory region
 */
void *shm_attach_region(shm_region_t region, 
                        const void *attach_addr, 
                        int shm_flag
                       )
{
    void *shm; 
    if((shm = shmat(region->id, attach_addr, shm_flag)) == (void *) -1) {
        perror("shmat");
        exit(1);
    }
    region->addr = (char *)shm;

    // TODO: page alignment
    region->base_addr = region->addr;

    return shm;
}

/*
 * get a handle of shared memroy region created by some other process
 */
shm_region_t shm_get_region(key_t key, size_t size, int shm_flag)
{
    shm_region_t region = (shm_region_t) malloc(sizeof(shm_region));
    if(!region) {
        fprintf(stderr, "cannot allocate memory. %s%d\n", __FILE__, __LINE__);
        return NULL;
    }
    memset(region, 0, sizeof(shm_region));

    if((region->id = shmget(key, size, shm_flag)) < 0) {
        perror("shmget");
        fprintf(stderr, "error no: %d\n", errno);
        free(region);
        return NULL;
    }
    region->key = key;
    region->size = size;
    region->shm_flag = shm_flag;
    return region;
}

/*
 * dettach from a shared memory region
 */
int shm_dettach_region(shm_region_t region)
{
    int rc = shmdt(region->addr);
    if(rc) {
        perror("shmdt");
        fprintf(stderr, "The shmdt call failed, error number = %d\n", errno);
    }
    return rc;
}

/*
 * delete a shared memory region. 
 * This should be called by the process which creates this region
 */
int shm_delete_region(shm_region_t region)
{
    // TODO: delete the shm region
    int rc = shmctl(region->id, IPC_RMID, (struct shmid_ds *)NULL);
    if(rc) {
        perror("shmctl");
        fprintf(stderr, "The shmctl call failed, error number = %d\n", errno);
    }
    rc = shmdt(region->addr);
    if(rc) {
        perror("shmdt");
        fprintf(stderr, "The shmdt call failed, error number = %d\n", errno);
    }
    free(region);
    return 0;
}
