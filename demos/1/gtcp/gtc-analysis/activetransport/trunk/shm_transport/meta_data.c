/*
 * ADIOS compatible meta-data structure for file/group/variables/attributes
 *
 * The meta-data structures for file/group/variable/attributes are essentially
 * a combination of those defined in adios_internals.h (for writing) and 
 * adios_read.h (for reading)
 *
 * NOTE: we assume that 1) each file can have one or multiple group; 2) each 
 * group can have one or multiple timesteps; and 3) The number of writers 
 * writing to a group is static.
 * Those assumptions hold for all current application scenarios. 
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
//#include "ffs.h"
#include "adios_types.h"
#include "adios_internals.h"
#include "shm_mm.h"
#include "shm_malloc.h"
#include "shm_queue.h"
#include "meta_data.h"
#include "shm_spinlock.h"

// global handle to meta data region
shm_meta_region_t meta_region = NULL;

/*
 * create meta-data region
 */
int create_meta_region(int total_size, int max_writers)
{
    if(!meta_region) {
        meta_region = (shm_meta_region_t) malloc(sizeof(shm_meta_region));
        if(!meta_region) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
    }
    else {
        return -1;
    }

    meta_region->region = shm_create_region(META_REGION_KEY, total_size, META_REGION_FLAG); 
    if(!meta_region->region) {
        fprintf(stderr, "cannot create meta region. %s:%d\n", __FILE__, __LINE__);
        return -1;
    }

    // create a shared memory region at a well known location
    meta_region->meta_data = (meta_data_region_t) 
        shm_attach_region(meta_region->region, NULL, META_REGION_FLAG);
    if(!meta_region->meta_data) {
        free(meta_region);
        meta_region = NULL;
        return -1;
    }
    meta_region->meta_data->total_size = total_size;
    // maximal possible size of an entry
    meta_region->meta_data->entry_size = sizeof(shm_file_index) + max_writers * sizeof(shm_queue_info);
    meta_region->meta_data->num_files = 0;
//    meta_region->meta_data->rw_lock = UNLOCKED; // TODO: initialize lock
    spinlock_init(&meta_region->meta_data->rw_lock);// TODO: initialize lock
    meta_region->meta_data->next = 0;

    return 0;
}

/*
 * destroy meta-data region. must be called by the process creating the region
 */
int destroy_meta_region()
{
    int rc = shm_delete_region(meta_region->region);
    if(rc) return rc;
    free(meta_region);
    meta_region = NULL;
    return 0;
}

/*
 * initialize handle to meta data region. called by other processes
 */
int initialize_meta_region(int total_size)
{
    if(!meta_region) {
        meta_region = (shm_meta_region_t) malloc(sizeof(shm_meta_region));
        if(!meta_region) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
    }
    else {
        return -1;
    }

    meta_region->region = shm_create_region(META_REGION_KEY, total_size, META_REGION_FLAG);
    if(!meta_region->region) {
        return -1;
    }

    // get shared memory region at a well known location
    meta_region->region = shm_get_region(META_REGION_KEY, total_size, META_REGION_FLAG);
    if(!meta_region->region) {
        return -1;
    }
    meta_region->meta_data = (meta_data_region_t)
        shm_attach_region(meta_region->region, NULL, META_REGION_FLAG);
    if(!meta_region->meta_data) {
        return -1;
    }

    return 0;
}

/*
 * cleanup handle to meta data region. called by other processes
 */
int finalize_meta_region()
{
    int rc = shm_dettach_region(meta_region->region);
    if(rc) return rc;
    free(meta_region);
    meta_region = NULL;
    return 0;
}

/*
 * TODO: create a unique key based on file name and rank
 */
unsigned int generate_region_key(char *file_name, char *group_name, int rank)
{
    return (rank % 16) + 6000;
}

/*
 * create a data queue per writer per file
 */
shm_data_queue_t create_data_queue(char *file_name, 
                                   char *group_name, 
                                   int mpi_rank, 
                                   size_t region_size,
                                   int is_static,
                                   int num_slots,
                                   int chunk_size
                                  )
{
    char c;
    int shmid;
    char *shm;

    // TODO: create a unique key based on file name and rank
    key_t key = generate_region_key(file_name, group_name, mpi_rank);
    int shm_flag = DATA_QUEUE_FLAG;

    // creates a shm region
    shm_region_t region = shm_create_region(key, region_size, shm_flag);
    if(!region) {
        return NULL;
    }

    // attach the segment to our data space.
    shm = shm_attach_region(region, NULL, 0);
    if(!shm) {
        return NULL;
    }

    // set memory allocator for a shared memory region
    shm_malloc_t mm = shm_associate_allocator(region,
                                     &default_allocator,
                                     NULL 
                                    );

    // write queue parameters at the beginning
    uint64_t *queue_offset = (uint64_t *)shm;
    int *int_ptr = (int *)(shm + sizeof(uint64_t));
    *int_ptr = is_static;
    *(int_ptr+1) = num_slots;
    *(int_ptr+2) = chunk_size;
    *(int_ptr+3) = strlen(group_name);
    char *current_pos = shm + 4*sizeof(int) + sizeof(uint64_t);
    memcpy(current_pos, group_name, strlen(group_name)+1);
    *queue_offset = 4*sizeof(int) + sizeof(uint64_t) + strlen(group_name) + 1;
    uint64_t padding = *queue_offset % CACHE_LINE_SIZE;
    if(padding) {
        *queue_offset += CACHE_LINE_SIZE - padding;
    }

    // create a shm queue
    // TODO: set parameters
    shm_q_handle_t q = shm_q_create(1,
                                    num_slots,
                                    mm,
                                    *queue_offset,      
                                    num_slots * chunk_size, // TODO
                                    is_static,
                                    chunk_size 
                                   );

    shm_data_queue_t data_q = (shm_data_queue_t) 
        malloc(sizeof(shm_data_queue));
    if(!data_q) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }
    data_q->region = region;
    data_q->mm = mm;
    data_q->queue = q;
    return data_q;
}

/*
 * delete local data queue by writer
 */
int delete_data_queue(shm_data_queue_t data_q)
{
    shm_q_destroy(data_q->queue);
    shm_delete_allocator(data_q->mm);
    shm_delete_region(data_q->region);
    free(data_q);
    return 0;
}

/*
 * dettach data queue by reader
 */
int dettach_data_queue(shm_data_queue_t data_q)
{
    shm_q_destroy(data_q->queue);
    shm_delete_allocator(data_q->mm);
    shm_dettach_region(data_q->region);
    free(data_q);
    return 0;
}

/*
 * create a new file entry in meta-data region for the first timestep
 */
shm_file_index_t create_new_file(char *file_name,
                                 char *group_name,
                                 int start_tidx,
                                 enum ADIOS_FLAG host_language_fortran,
                                 int mpi_rank,
                                 shm_data_queue_t data_q,
                                 shm_coordinator_t coordinator
                                )
{
    int myid = shm_coordinator_myid(coordinator);
    shm_file_index_t entry;
    char *queues;

    // master writer lock the meta-data region and add a new entry
    if(myid == 0) {
        // TODO: need to implement a real lock
//        while(meta_region->meta_data->rw_lock != 0);       
//        meta_region->meta_data->rw_lock = 1; 
        spinlock_lock(&meta_region->meta_data->rw_lock);
      
        // find the location for new file entry 
        entry = (shm_file_index_t)((uint64_t)meta_region->meta_data->contents + meta_region->meta_data->next);
        memcpy(entry->file_name, file_name, strlen(file_name)+1);              
        memcpy(entry->group_name, group_name, strlen(group_name)+1);              

        // TODO: not spinlock, a read-write lock
//        entry->rw_lock = 1;
        spinlock_init(&entry->rw_lock);
//        spinlock_lock(&entry->rw_lock);

        entry->file_state = file_state_writing;
        entry->mode = adios_mode_write;
        entry->host_language_fortran = host_language_fortran;
        entry->timestep = start_tidx;
        entry->ntimesteps = 1;
        entry->lasttimestep = start_tidx;
        entry->tidx_start = start_tidx;
        entry->num_writers = shm_coordinator_size(coordinator);
        queues = entry->contents; 
        uint64_t entry_addr = (uint64_t)entry - (uint64_t)meta_region->meta_data;         

        // let other writers to put their queue info into the entry  
        shm_bcast(coordinator, 0, (void *)&entry_addr, sizeof(uint64_t));

        // TODO: memory fence
    }
    else {
        // wait for my turn to put queue info into the entry
        uint64_t entry_addr = 0;
        shm_bcast(coordinator, 0, (void *)&entry_addr, sizeof(uint64_t));

        entry = (shm_file_index_t)((char *)meta_region->meta_data + entry_addr);    
        queues = entry->contents;
    }

    // every writer puts queue info into the proper location in entry
    shm_queue_info_t my_queue = (shm_queue_info_t)(queues + sizeof(shm_queue_info) * myid);
    my_queue->writer_rank = mpi_rank;
    my_queue->id = data_q->region->id;
    my_queue->key = data_q->region->key;
    my_queue->size = data_q->region->size;
    my_queue->flag = data_q->region->shm_flag;

    shm_barrier(coordinator);

    // update file state
    if(myid == 0) {
//        entry->rw_lock = 0;
//        spinlock_lunock(&entry->rw_lock);

        entry->file_state = file_state_written;

//        meta_region->meta_data->next = (uint64_t)my_queue + 
//            entry->num_writers*sizeof(shm_queue_info) - (uint64_t)(meta_region->meta_data);
        meta_region->meta_data->next = (uint64_t)my_queue ; 
        meta_region->meta_data->next += entry->num_writers*sizeof(shm_queue_info);
        meta_region->meta_data->next -= (uint64_t)(meta_region->meta_data);

        // padding for cache line boundary
        uint64_t padding_size = meta_region->meta_data->next % CACHE_LINE_SIZE;
        if(padding_size) {
            meta_region->meta_data->next += CACHE_LINE_SIZE - padding_size;
        }

        // release lock
//        meta_region->meta_data->rw_lock = 0;
        spinlock_unlock(&meta_region->meta_data->rw_lock);
    }
    return entry;
}

/*
 * update meta-data entry for a new timestep
 */
int append_to_file(shm_file_index_t file,
                   int current_tidx,
                   shm_coordinator_t coordinator
                   )
{
    int myid = shm_coordinator_myid(coordinator);

    shm_barrier(coordinator);
    if(myid == 0) {
        // lock the file entry   
//        while(file->rw_lock != 0);     
//        file->rw_lock = 1; 
        spinlock_lock(&file->rw_lock);

        file->file_state = file_state_written;
        file->mode = adios_mode_append;
        file->timestep = current_tidx;
        file->ntimesteps ++;

        // unlock
//        file->rw_lock = 0;
        spinlock_unlock(&file->rw_lock);
    }    
    return 0;
}

/*
 * test file state. non-blocking call
 */
int is_file_ready_to_write(shm_file_index_t file,
                           shm_coordinator_t coordinator
                          )
{
    int myid = shm_coordinator_myid(coordinator);

    // lock the file entry
    spinlock_lock(&file->rw_lock);
    int rc = (file->file_state != file_state_reading);
    spinlock_unlock(&file->rw_lock);
    return rc;
}

int wait_for_writing(shm_file_index_t file, shm_coordinator_t coordinator)
{
    int myid = shm_coordinator_myid(coordinator);

    if(myid == 0) {
        while(1) {
            // lock the file entry
            spinlock_lock(&file->rw_lock);

            if(file->file_state == file_state_reading) { // we have to wait
                spinlock_unlock(&file->rw_lock);
                // TODO: back off

                continue;
            }
            else {
                file->file_state = file_state_writing;
                spinlock_unlock(&file->rw_lock);
                break;
            }
        }
    }
    shm_barrier(coordinator);
    return 0;
}

/*
 * mark the end of file, meaning there is no more data to write
 */
int close_file_w(shm_file_index_t file, shm_coordinator_t coordinator)
{
    int myid = shm_coordinator_myid(coordinator);

    if(myid == 0) {
        // TODO: we need to lock the whole meta-data region and recyle the entry

        // lock the file entry
//        while(file->rw_lock != 0);
//        file->rw_lock = 1;
        while(1) {
            spinlock_lock(&file->rw_lock);

            // make sure all timesteps have been read
            if(file->file_state == file_state_read &&
               file->lasttimestep == file->timestep + 1) {

                file->file_state = file_state_EOF;
                spinlock_unlock(&file->rw_lock);
                break;
            }
            else { // wait 
                spinlock_unlock(&file->rw_lock);
                // TODO: back off
                sleep(1);
            }
        }
    }
    shm_barrier(coordinator);
    return 0;
}

/*
 * look up file by name in meta-data region
 * this is only used for the first time
 * this is is a non-blocking call: if file is not present,
 * returns NULL.
 */
shm_file_index_t get_file_byname(char *file_name,
                                 shm_coordinator_t coordinator
                                )
{
    int myid = shm_coordinator_myid(coordinator);
    shm_file_index_t file = NULL;
    uint64_t entry_offset;

    if(myid == 0) {
        // TODO: need to implement a real lock
//        while(meta_region->meta_data->rw_lock != 0);
//        meta_region->meta_data->rw_lock = 2;
        spinlock_lock(&meta_region->meta_data->rw_lock);

        entry_offset = (uint64_t) ((char *)meta_region->meta_data->contents - (char *)meta_region->meta_data);
        while(entry_offset < meta_region->meta_data->next) {
            shm_file_index_t entry = (shm_file_index_t)(entry_offset + (char *)meta_region->meta_data);
  
            spinlock_lock(&entry->rw_lock);  

            if(!strcmp(file_name, entry->file_name)) {
                file = entry;
                   
                // return while holding the lock on the file
                break;
            }

            spinlock_unlock(&entry->rw_lock);  

            // TODO: we use fixed sized entry
            entry_offset += meta_region->meta_data->entry_size;
        }
        if(file) {
            // TODO: memroy fence
            shm_bcast(coordinator, 0, (void *)&entry_offset, sizeof(uint64_t));
        } 
        else {
            uint64_t t = 0;
            shm_bcast(coordinator, 0, (void *)&t, sizeof(uint64_t));
        }        
    }
    else {
        // wait for signal from master
        shm_bcast(coordinator, 0, (void *)&entry_offset, sizeof(uint64_t));
        if(entry_offset != 0) {
            file = (shm_file_index_t)((char *)meta_region->meta_data + entry_offset);
        }
    }

    shm_barrier(coordinator);

    if(myid == 0) {
        // release lock
//        meta_region->meta_data->rw_lock = 0;
        spinlock_unlock(&meta_region->meta_data->rw_lock);
    }

    return file;
}

/*
 * lock file for reading
 */
int lock_file_by_reader(shm_file_index_t file,
                        shm_coordinator_t coordinator
                       )
{
    int myid = shm_coordinator_myid(coordinator);
    if(myid == 0) {
//        while(file->rw_lock != 0);
//        file->rw_lock = 2;
        spinlock_lock(&file->rw_lock);
    } 
    shm_barrier(coordinator);

    return 0;
}

/*
 * unlock file
 */
int unlock_file_by_reader(shm_file_index_t file,
                          shm_coordinator_t coordinator
                         )
{
    int myid = shm_coordinator_myid(coordinator);
    if(myid == 0) {
//        file->rw_lock = 0;
        spinlock_unlock(&file->rw_lock);
    }
    shm_barrier(coordinator);

    return 0;
}

/*
 * get a handle of each writer's data queue
 * this is only called for the first timestep
 */
shm_data_queue_r_t get_data_queues(shm_file_index_t file)
{
    shm_data_queue_r_t data_queues = 
        (shm_data_queue_r_t) malloc(file->num_writers * sizeof(shm_data_queue_r));
    if(!data_queues) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }        

    // attach each writer's shared memory region
    // TODO: selectively attach those regions which will actually be read
    shm_queue_info_t queue_info = (shm_queue_info_t) file->contents;
    int i;
    for(i = 0; i < file->num_writers; i ++) {
        data_queues[i].writer_rank = queue_info[i].writer_rank;

        data_queues[i].region = shm_get_region(queue_info[i].key,
                                               queue_info[i].size, 
                                               queue_info[i].flag
                                              );
        if(!data_queues[i].region) {
            return NULL;
        }

        char *shm = shm_attach_region(data_queues[i].region, NULL, 0);
        if(!shm) {
            return NULL;
        }

        data_queues[i].mm = shm_associate_allocator(data_queues[i].region,
                                                    &default_allocator,
                                                    NULL
                                                   );
             
        // TODO: get queue parameters
        uint64_t queue_offset = *((uint64_t *)shm);
        int *int_ptr = (int *)(shm + sizeof(uint64_t));
        int is_static = *int_ptr;
        int num_slots = *(int_ptr + 1); 
        int chunk_size = *(int_ptr + 2); 
        int group_name_len = *(int_ptr + 3); 

        data_queues[i].queue = shm_q_create(0,
                                            num_slots,  
                                            data_queues[i].mm,
                                            queue_offset,  
                                            num_slots*chunk_size,    // TODO
                                            is_static, 
                                            chunk_size
                                           );
    }
    return data_queues;  
}

/*
 * delete data queues
 */
int close_data_queues(shm_data_queue_r_t data_queues, int num_pgs)
{
    int i;
    for(i = 0; i < num_pgs; i ++) {
        shm_q_destroy(data_queues[i].queue);
        shm_delete_allocator(data_queues[i].mm);

        // TODO: detach and delete region
        shm_dettach_region(data_queues[i].region);
    }
    free(data_queues);
    return 0;
}

/*
 * test for new timestep to read
 */
int is_new_timestep_ready(shm_file_index_t file, 
                          int tidx,
                          shm_coordinator_t coordinator
                         )
{
     lock_file_by_reader(file, coordinator);
     int rc = (file->lasttimestep <= tidx && file->timestep >= tidx)? 1:0; 
     unlock_file_by_reader(file, coordinator);
     return rc;
}

/*
 * poll for new timestep to read
 */
enum FILE_STATE get_new_timestep(shm_file_index_t file, 
                     int tidx,
                     shm_coordinator_t coordinator 
                    )
{
    // lock file
    lock_file_by_reader(file, coordinator);

    // if new timestep is ready, return while holding the lock
    if(file->lasttimestep <= tidx && file->timestep >= tidx) {
        file->file_state = file_state_reading;
        return file_state_reading;
    }
    else if(file->file_state == file_state_EOF) { // End of file: writer has finished
        // TODO: return while holding the lock
//        unlock_file_by_reader(file, coordinator);
        return file_state_EOF;
    }
    else {
        // otherwise unlock the file and return
        unlock_file_by_reader(file, coordinator);
        return file_state_unavail;
    }
}

/*
 * finish reading current timestep and advance to the next timestep
 */
int finish_read_timestep(shm_file_index_t file,
                         shm_coordinator_t coordinator
                        )
{
    // lock file
 
    int myid = shm_coordinator_myid(coordinator);
    if(myid == 0) {
        file->lasttimestep ++;
        file->file_state = file_state_read;
    }
    unlock_file_by_reader(file, coordinator);
    return 0;
}

/*
 * finish reading the file
 */
int close_file_r(shm_file_index_t file, shm_coordinator_t coordinator)
{
    // TODO: when there are multiple readers, we need reference count
    int myid = shm_coordinator_myid(coordinator);
    if(myid == 0) {
        file->file_state = file_state_done;
    }
    unlock_file_by_reader(file, coordinator);
    return 0;
}

