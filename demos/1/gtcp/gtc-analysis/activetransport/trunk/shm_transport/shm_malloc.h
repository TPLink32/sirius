#ifndef _SHM_MALLOC_H_
#define _SHM_MALLOC_H_
/*
 * Dynamic memory allocation in shared memory regions
 * 
 */ 
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include "shm_mm.h"

/*
 * interface definition of a memory allocator
 */ 
struct _shm_allocator;
struct _shm_malloc;

typedef int (* shm_malloc_init_func)(shm_region_t, void *, void **);

typedef void * (* shm_malloc_at_func)(shm_region_t, void *, void *, size_t);

typedef void * (* shm_realloc_func)(shm_region_t, void *, void *, size_t);

typedef void (* shm_free_func)(shm_region_t, void *, void *);

typedef void (* shm_memstats_func)(shm_region_t, void *);

typedef int (* shm_malloc_finalize_func)(shm_region_t, void *); 

typedef struct _shm_allocator {
    shm_malloc_init_func init_func;
    shm_malloc_at_func malloc_at_func;
    shm_realloc_func realloc_func;
    shm_free_func free_func;
    shm_memstats_func stats_func;
    shm_malloc_finalize_func finalize_func;
} shm_allocator, *shm_allocator_t;

/*
 * dynamic memory allocator for a shared memory region
 */
typedef struct _shm_malloc {
    shm_region_t region;
    void *private_data;
    shm_allocator_t allocator;
} shm_malloc, *shm_malloc_t;

/*
 * set memory allocator for a shared memory region
 */
shm_malloc_t shm_associate_allocator(shm_region_t region, 
                                     shm_allocator_t allocator, 
                                     void *arg
                                    );

/*
 * delete memory allocator for a shared memory region
 */
int shm_delete_allocator(shm_malloc_t m);
 
/*
 * dynamically allocate memory at specific location within a shared memory region
 */
void *shm_malloc_at(shm_malloc_t m, void *start_addr, size_t size);

/*
 * dynamically re-allocate memory within a shared memory region
 */
void *shm_realloc(shm_malloc_t m, void *addr, size_t size);

/*
 * dynamically recyle memory within a shared memory region
 */
void shm_free(shm_malloc_t m, void *addr);


/*
 * declare default memory allocator
 */
#ifndef DEFAULT_ALLOCATOR_DEF

extern shm_allocator default_allocator;

#endif

#ifdef __cplusplus
}
#endif
  
#endif 
 
