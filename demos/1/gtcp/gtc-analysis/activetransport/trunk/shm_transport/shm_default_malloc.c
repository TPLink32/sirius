/*
 * default memory allocator for dynamic management within shared memory region
 */
#define DEFAULT_ALLOCATOR_DEF

#include "stdio.h"
#include "stdlib.h"
#include "shm_mm.h"
#include "shm_malloc.h"

// forward declaration
int default_malloc_init(shm_region_t, void *, void **);
void *default_malloc_at(shm_region_t, void*, void *, size_t);
void *default_realloc(shm_region_t, void *, void *, size_t);
void default_free(shm_region_t, void *, void *);
void default_memstats(shm_region_t, void *);
int  default_malloc_finalize(shm_region_t, void *);

/*
 * default memory allocator
 */
shm_allocator default_allocator = {
    .init_func = default_malloc_init,
    .malloc_at_func = default_malloc_at,
    .realloc_func = default_realloc,
    .free_func = default_free, 
    .stats_func = default_memstats,
    .finalize_func = default_malloc_finalize
};

/*
 * initialize
 */
int default_malloc_init(shm_region_t region, void *arg, void **private_data)
{
    // TODO
    private_data = NULL;
    return 0;
}

/*
 * allocate a contiguous block starting at specified address
 */
void *default_malloc_at(shm_region_t region, void *private_data, void *start_addr, size_t size)
{
    // TODO
    return start_addr;
}

/*
 * re-allocate a contiguous block with specified size
 */
void *default_realloc(shm_region_t region, void *private_data, void *addr, size_t size)
{
    // TODO
    return addr;
}

/*
 * recycle a previously allocated block
 */
void default_free(shm_region_t region, void *private_data, void *addr)
{
    // TODO
    return;
}

/*
 * print out statistics
 */
void default_memstats(shm_region_t region, void *private_data)
{
    // TODO
    printf("Nothing to report.\n");
}

/*
 * finalize
 */
int default_malloc_finalize(shm_region_t region, void *private_data)
{
    // TODO
    return 0;
}


