#ifndef _SHM_MM_H_
#define _SHM_MM_H_
/*
 * Low level shared memory management functions. 
 *
 */
 
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 

/*
 * a shared memory region 
 */
typedef struct _shm_region {
    int id;              // unique id of the shared memory segment returned by shmget()     
    key_t key;           // key used to create this shared memory region
    size_t size;         // total size of the region in bytes
    int shm_flag;        // flag used to create/attach this region
    char *addr;          // attaching address
    char *base_addr;     // page aligned starting address
    int is_creator;      // this this region created by myself
} shm_region, *shm_region_t;

/*
 * create a shared memory region 
 */ 
shm_region_t shm_create_region(key_t key, size_t size, int shm_flag);
 
/*
 * get a handle of shared memroy region created by some other process
 */
shm_region_t shm_get_region(key_t key, size_t size, int shm_flag);

/*
 * attach to a created shared memory region
 */
void *shm_attach_region(shm_region_t region,
                        const void *attach_addr,
                        int shm_flag
                       );

/*
 * dettach from a shared memory region
 */
int shm_dettach_region(shm_region_t region);

/*
 * delete a shared memory region. 
 * This should be called by the process which creates this region
 */
int shm_delete_region(shm_region_t region);

#ifdef __cplusplus
}
#endif
 
#endif

 
