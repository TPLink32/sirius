#ifndef _SHM_QUEUE_H_
#define _SHM_QUEUE_H_
/*
 * A low level shared memory queue implementation
 *
 * This library is designed as building block for ADIOS shared memory
 * tranport. 
 *   
 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "shm_config.h"
#include "shm_mm.h"
#include "shm_malloc.h"

/*
 * default values 
 */
#define SLOT_DATA_LENGTH (32*1024*1024) // 32MB
#define QUEUE_LENGTH       8      
#define NUM_PRODUCERS    1
#define NUM_CONSUMERS    1

/*
 * data structure of queue in shared memory
 */

/*
 * slot status: empty (ready for writing) or full (ready for reading)
 */ 
enum SLOT_FLAG{SLOT_FULL  = 0,
               SLOT_EMPTY = 1
               };
  
/*
 * one slot in the share memory queue
 */
typedef struct _shm_q_slot
{
    enum SLOT_FLAG is_empty;      // 1: slot is empty; 0: slot is full
    uint64_t size;                // size of payload in bytes
    uint64_t data;                // offset of actual data payload of 'size' bytes
    
    // padding to be cache line aligned
    char padding[CACHE_LINE_SIZE - sizeof(enum SLOT_FLAG) - sizeof(uint64_t) - sizeof(void *)];    
} shm_q_slot, *shm_q_slot_t;

/*
 * bookkeeping data structure in sender and receiver's local memory
 */
typedef struct _shm_q_handle {
    int is_sender;                // sender side (1) or receiver side (0)
    shm_q_slot_t *slots;          // point to starting address of queue in shared memory 
    int slot_index;               // index of current slot to operate on
    int num_slots;                // set during intialization and remains static    
    int is_static_size;           // is slot size static&equal (1) or varing dynamically (0)
    uint64_t slot_size;           // size of each slot. this is only used when slots are statically equally sized
    uint64_t start_offset;        // starting address relative to region->base_addr;
    uint64_t payload_start_offset;// starting address of payload storage area, relative to region->base_addr
    uint64_t total_size;          // actual size of all slots in bytes 
    shm_malloc_t mm;              // dynamic memory allocator for the shared memroy region in which the queue resides
} shm_q_handle, *shm_q_handle_t;
    
/*
 * functions used to manipualte shared memory queue
 */
    
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
                            );
                            
/*
 * put a chunk of data into queue (called by sender) 
 */
int shm_q_put(shm_q_handle_t queue, void *data, uint64_t length);

/*
 * get reference of current slot (called by sender or receiver)
 */
shm_q_slot_t shm_q_get_current_slot(shm_q_handle_t queue);

/*
 * set current slot (called by sender)
 */
void shm_q_set_current_slot_full(shm_q_handle_t queue);

/*
 * get address of data in current slot
 */
void *shm_q_get_data_in_slot(shm_q_handle_t queue, shm_q_slot_t s);

/*
 * get offset relative to shared memory region's base address
 */
uint64_t shm_q_get_offset(shm_q_handle_t queue, void *addr);

/*
 * get address from offset relative to shared memory region's base address
 */
void *shm_q_get_addr(shm_q_handle_t queue, uint64_t offset);

/*
 * mark the current slot as it has been read
 */
void shm_q_release_current_slot(shm_q_handle_t queue);
 
/*
 * test if current slot is empty
 */ 
int shm_q_is_empty(shm_q_handle_t queue);

/*
 * test if current slot is full (has data in it)
 */
int shm_q_is_full(shm_q_handle_t queue);

/*
 * destroy a shared memory queue
 */
int shm_q_destroy(shm_q_handle_t queue);
#ifdef __cplusplus
}
#endif
  
#endif
 

