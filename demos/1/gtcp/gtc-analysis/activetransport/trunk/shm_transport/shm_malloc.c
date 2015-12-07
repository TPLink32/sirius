/*
 * Dynamic memory allocation in shared memory regions
 * 
 */ 

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "shm_mm.h"
#include "shm_malloc.h"

/*
 * set memory allocator for a shared memory region
 */
shm_malloc_t shm_associate_allocator(shm_region_t region, 
                                     shm_allocator_t allocator, 
                                     void *arg
                                    )
{
    // TODO
    shm_malloc_t m = (shm_malloc_t) malloc(sizeof(shm_malloc));
    if(!m) {
        fprintf(stderr, "Cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }
    m->region = region;
    m->allocator = allocator;
    
    // initialize the memory allocator
    int rc = (* (allocator->init_func))(region, arg, &(m->private_data));
    if(rc == 0) {
        return m;
    }
    else {
        return NULL;
    }
}

/*
 * delete memory allocator for a shared memory region
 */
int shm_delete_allocator(shm_malloc_t m)
{
    int rc = (* (m->allocator->finalize_func))(m->region, m->private_data);    
    free(m);
    return rc;
}

/*
 * dynamically allocate memory at specific location within a shared memory region
 */
void *shm_malloc_at(shm_malloc_t m, void *start_addr, size_t size)
{
    return (* (m->allocator->malloc_at_func))(m->region, m->private_data, start_addr, size);
}

/*
 * dynamically re-allocate memory within a shared memory region
 */
void *shm_realloc(shm_malloc_t m, void *addr, size_t size)
{
    return (* (m->allocator->realloc_func))(m->region, 
                                           m->private_data, 
                                           addr, 
                                           size
                                           );
}

/*
 * dynamically recyle memory within a shared memory region
 */
void shm_free(shm_malloc_t m, void *addr)
{
    (* (m->allocator->free_func))(m->region, m->private_data, addr);
}
