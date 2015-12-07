/*
 * A FIFO, circular, lock free, shared memory queue implementation
 *
 * This library is designed as building block for ADIOS shared memory
 * tranport. 
 *   
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "shm_mm.h"
#include "shm_malloc.h"
#include "shm_queue.h"
    
/*
 * create a shared memory queue (called by sender or receiver)
 */ 
shm_q_handle_t shm_q_create(int is_sender, 
                             int num_slots, 
                             shm_malloc_t mm,
                             uint64_t start_offset,
                             uint64_t total_size,
                             int is_static_size,
                             uint64_t slot_size                             
                            )
{
    int i;
    
    shm_q_handle_t q_handle = (shm_q_handle_t) malloc(sizeof(shm_q_handle));    
    if(!q_handle) {
        fprintf(stderr, "Cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }
    q_handle->is_sender = is_sender;
    q_handle->num_slots = num_slots;
    q_handle->mm = mm;
    q_handle->total_size = total_size;
    q_handle->is_static_size = is_static_size;
    q_handle->slot_size = slot_size;
    q_handle->slot_index = 0;
    
    // TODO: assert that starting_addr is within the shared memory region
    if(start_offset < 0 || start_offset > mm->region->size) {
        fprintf(stderr, "Starting offset is not valid. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }
    
    char *start = mm->region->base_addr + start_offset;
    q_handle->slots = (shm_q_slot_t *) malloc(num_slots * sizeof(shm_q_slot_t));          
    if(!q_handle->slots) {
        fprintf(stderr, "Cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }        
        
    if(is_sender) { // sender will initialize the queue    
        if(!is_static_size) {
            // dynamically allocate headers in share memory region
            void *head_addr = shm_malloc_at(mm, 
                mm->region->base_addr + start_offset, num_slots * sizeof(shm_q_slot));
            if(!head_addr) {
                fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
                return NULL;
            }
        }        
        for(i = 0; i < num_slots; i ++) {
            q_handle->slots[i] = (shm_q_slot_t)(start + i * sizeof(shm_q_slot));
            q_handle->slots[i]->is_empty = SLOT_EMPTY;
            q_handle->slots[i]->data = 0;
        }    
        
        if(is_static_size) {        
            // payload starts right after the headers
            uint64_t end_offset = start_offset + num_slots * sizeof(shm_q_slot);
            for(i = 0; i < num_slots; i ++) {
                // NOTE: use offset rather than address!
                // TODO: page alignment
                q_handle->slots[i]->data = end_offset + i * slot_size; 
            }
            q_handle->payload_start_offset = end_offset;
            
            // TODO: page alignment
            q_handle->total_size = start_offset + num_slots * sizeof(shm_q_slot)
               + num_slots * slot_size;
        }    
    }
    else { // receiver only needs to initialize local bookkeeping data
        for(i = 0; i < num_slots; i ++) {
            q_handle->slots[i] = (shm_q_slot_t)(start + i * sizeof(shm_q_slot));
        }
    }
    return q_handle;
}

/*
 * put a chunk of data into queue (called by sender) 
 */
int shm_q_put(shm_q_handle_t queue, void *data, uint64_t length)
{
    if(queue->is_sender) {
        int i = queue->slot_index;
        shm_q_slot_t s = queue->slots[i];
        if(queue->is_static_size) {
            if(length <= queue->slot_size) {
                memcpy((s->data + queue->mm->region->base_addr), data, length);
            }
            else {
                fprintf(stderr, "payload size (%lu) exceed slot size (%lu)\n", 
                    length, queue->slot_size);
                return -1;
            }
        }
        else { // dynamically allocate an area in shared memory region
            // TODO: we need a very simple way to track the circular buffer space    
            void *addr = shm_malloc_at(queue->mm, NULL, length);
            if(!addr) {
                fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
                return -1;
            }
            memcpy(addr, data, length);
            s->data = (char *)addr - queue->mm->region->base_addr;
        }                
        s->size = length;
        s->is_empty = SLOT_FULL;  // this will mark slot available for reading

        // TODO: memory fence
         
        queue->slot_index = (i + 1) % queue->num_slots;
        return 0;
    }
    else {
        fprintf(stderr, "Only sender can put data into queue.\n");
        return -1;
    }
}

/*
 * get reference of current slot (called by sender or receiver)
 */
shm_q_slot_t shm_q_get_current_slot(shm_q_handle_t queue)
{
    return queue->slots[queue->slot_index];
}

/*
 * get address of data in current slot
 */
void *shm_q_get_data_in_slot(shm_q_handle_t queue, shm_q_slot_t s)
{
    return (s->data + queue->mm->region->base_addr);
}

/*
 * get offset relative to shared memory region's base address
 */
uint64_t shm_q_get_offset(shm_q_handle_t queue, void *addr)
{
    return ((uint64_t)addr - (uint64_t)queue->mm->region->base_addr);
}

/*
 * get address from offset relative to shared memory region's base address
 */
void *shm_q_get_addr(shm_q_handle_t queue, uint64_t offset)
{
    return (offset + queue->mm->region->base_addr);
}

/*
 * set current slot (called by sender)
 */
void shm_q_set_current_slot_full(shm_q_handle_t queue)
{
    queue->slots[queue->slot_index]->is_empty = SLOT_FULL;
    // TODO: memory fence
    queue->slot_index = (queue->slot_index + 1) % queue->num_slots;
}

/*
 * mark the current slot as it has been read
 */
void shm_q_release_current_slot(shm_q_handle_t queue)
{
    if(queue->is_static_size) {
        queue->slots[queue->slot_index]->is_empty = SLOT_EMPTY;
        queue->slot_index = (queue->slot_index + 1) % queue->num_slots;
        
        // TODO: memory fence
    }
    else {
        void *addr = queue->slots[queue->slot_index]->data + queue->mm->region->base_addr;
        shm_free(queue->mm, addr);
        queue->slots[queue->slot_index]->is_empty = SLOT_EMPTY;
        // TODO: memory fence
    }
}
 
/*
 * test if current slot is empty
 */ 
int shm_q_is_empty(shm_q_handle_t queue)
{
    return (queue->slots[queue->slot_index]->is_empty == SLOT_EMPTY);
}

/*
 * test if current slot is full (has data in it)
 */
int shm_q_is_full(shm_q_handle_t queue)
{
    return (queue->slots[queue->slot_index]->is_empty == SLOT_FULL);
}

/*
 * destroy a shared memory queue
 */
int shm_q_destroy(shm_q_handle_t queue)
{
    if(queue->is_sender) {
        // sender needs to clear up the shared memory region
        if(queue->is_static_size) {
            // Nothing to do            
        }        
        else {
            // TODO: clean up dynamic memory allocator
            int i;
            void *addr;
            for(i = 0; i < queue->num_slots; i ++) {
                addr = queue->slots[i]->data + queue->mm->region->base_addr;            
                shm_free(queue->mm, addr);
            }
            addr = queue->start_offset + queue->mm->region->base_addr;
            shm_free(queue->mm, addr);
        }
    }
    else { 
        // receiver doesn't need to care about cleanup shared memory
    }
    free(queue->slots);
    free(queue);
    return 0;
}
