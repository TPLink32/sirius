#ifndef _SHM_META_DATA_H_
#define _SHM_META_DATA_H_
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
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
//#include "ffs.h"
#include "adios_types.h"
#include "adios_internals.h"
#include "shm_config.h"
#include "shm_mm.h"
#include "shm_malloc.h"
#include "shm_queue.h"
#include "shm_coordinator.h"

// default values
#define MAX_FILE_NAME_LENGTH 128   // limit of file name length 
#define MAX_CONCURRENT_TIMESTEPS 2 // limit of concurrent outstanding timesteps
#define MAX_FILES 5                // max number of files written by a program
#define MAX_WRITERS_PER_FILE 4     // max number of writer processes per file in a node
#define META_REGION_SIZE (500*1024)// meta data region size
#define META_REGION_KEY 5678       // meta data region
#define META_REGION_FLAG 0666      // meta data shm region flag for meta data region
#define DATA_QUEUE_FLAG 0666       // shm region flag for data queue
#define DATA_QUEUE_SLOT_SIZE (500*1024*1024) // size of one slot in data queue

/*
 * the following define the data structure and layout of meta-data and actual
 * data in shared memory region
 */

/*
 * file state
 */
enum FILE_STATE {file_state_writing  = 1,
                 file_state_written = 2,
                 file_state_reading = 3,
                 file_state_read = 4,
                 file_state_EOF = 5,
                 file_state_done = 6,
                 file_state_unavail = 7 // new timestep not available yet
                };

/*
 * point to each individual local data queue
 */ 
typedef struct _shm_queue_info 
{
    int writer_rank;
    int id;
    key_t key;
    size_t size;
    int flag;
    char padding[CACHE_LINE_SIZE - 3*sizeof(int) - sizeof(key_t) - sizeof(size_t)];
} shm_queue_info, *shm_queue_info_t;

/*
 * An file index structure for a distinct file buffered in local node
 */ 
typedef struct _shm_file_index
{
    char file_name[MAX_FILE_NAME_LENGTH];
    char group_name[MAX_FILE_NAME_LENGTH];
    int rw_lock;
    enum FILE_STATE file_state; 
    enum ADIOS_METHOD_MODE mode; 
    enum ADIOS_FLAG host_language_fortran;    

    // TODO: time index should be per-group
    // time index
    int32_t timestep;               // The current (only) timestep 
    int32_t ntimesteps;             // Number of timesteps in file. There is always at least one timestep 
    int32_t lasttimestep;           // The currently available latest timestep in the stream   
    int32_t tidx_start;             // First timestep in file, usually 1

    // point to individual writer's local data queue
    int32_t num_writers;
    char padding[CACHE_LINE_SIZE - sizeof(enum FILE_STATE) - sizeof(enum ADIOS_METHOD_MODE) - sizeof(char) - 5*sizeof(int32_t)];
    char contents[0];
} shm_file_index, *shm_file_index_t; 

/*
 * structure of meta-data region
 * A simple fixed sized array to organize file meta-data
 */
typedef struct _shm_meta_data_region
{
    int num_files;
    int total_size;
    int entry_size;
    int rw_lock;
    uint64_t next;
    char padding[CACHE_LINE_SIZE - 4*sizeof(int) - sizeof(uint64_t)];
    char contents[0];
} meta_data_region, *meta_data_region_t;


/*
 * the following define bookkeeping data structure in writer and reader's local memory
 */

/*
 * a handle to local meta data region
 */
typedef struct _shm_meta_region {
    shm_region_t region;
    meta_data_region_t meta_data;
} shm_meta_region, *shm_meta_region_t;

/*
 * a handle to local data queue at writer side
 */
typedef struct _shm_data_q_writer {
    // TODO: some shortcuts to point to current/next slot in queue    
    shm_region_t region;
    shm_malloc_t mm;
    shm_q_handle_t queue;  // end point to send data to queue 
} shm_data_queue, *shm_data_queue_t;

/*
 * a handle to local data queue at reader side
 */
typedef struct _shm_data_q_reader {
    // TODO: some shortcuts to point to current/next slot in queue    
    int writer_rank;
    shm_region_t region;
    shm_malloc_t mm;
    shm_q_handle_t queue;  // end point to receive data from queue    
} shm_data_queue_r, *shm_data_queue_r_t;

/*
 * manage meta-data region
 */

/*
 * create meta-data region
 */
int create_meta_region(int total_size, int max_writers);

/*
 * initialize handle to meta data region. called by other processes
 */
int initialize_meta_region(int total_size);

/*
 * destroy meta-data region. must be called by the process creating the region
 */
int destroy_meta_region();

/*
 * cleanup handle to meta data region. called by other processes
 */
int finalize_meta_region();


/*
 * functions at writer side to manipulate meta-data queue
 */

/*
 * TODO: create a unique key based on file name and rank
 */
unsigned int generate_region_key(char *file_name, char *group_name, int rank);

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
                                  );

/*
 * delete local data queue
 */
int delete_data_queue(shm_data_queue_t data_q);

/*
 * dettach data queue by reader
 */
int dettach_data_queue(shm_data_queue_t data_q);

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
                                );

/*
 * update meta-data entry for a new timestep
 */
int append_to_file(shm_file_index_t file, 
                   int current_tidx,
                   shm_coordinator_t coordinator
                   );

/*
 * test file state. non-blocking call
 */
int is_file_ready_to_write(shm_file_index_t file, 
                           shm_coordinator_t coordinator
                          );

int wait_for_writing(shm_file_index_t file, shm_coordinator_t coordinator);

/*
 * mark the end of file, meaning there is no more data to write
 */
int close_file_w(shm_file_index_t file, shm_coordinator_t coordinator);


/*
 * functions at reader side to manipulate meta-data queue
 */

/*
 * look up file by name in meta-data region
 * this is only used for the first time
 */
shm_file_index_t get_file_byname(char *file_name, 
                                 shm_coordinator_t coordinator
                                );

/*
 * lock file for reading
 */
int lock_file_by_reader(shm_file_index_t file, 
                        shm_coordinator_t coordinator
                       );

/*
 * unlock file
 */
int unlock_file_by_reader(shm_file_index_t file, 
                          shm_coordinator_t coordinator
                         );

/*
 * get a handle of each writer's data queue
 * this is only called for the first timestep
 */
shm_data_queue_r_t get_data_queues(shm_file_index_t file);

/*
 * delete data queues 
 */
int close_data_queues(shm_data_queue_r_t data_queues, int num_pgs);

/*
 * test for new timestep to read
 */
int is_new_timestep_ready(shm_file_index_t file, 
                          int tidx, 
                          shm_coordinator_t coordinator
                         );

/*
 * poll for new timestep to read
 */
enum FILE_STATE get_new_timestep(shm_file_index_t file, 
                     int tidx,
                     shm_coordinator_t coordinator
                    );

/*
 * finish reading current timestep and advance to the next timestep
 */
int finish_read_timestep(shm_file_index_t file, shm_coordinator_t coordinator);

/*
 * finish reading the file
 */
int close_file_r(shm_file_index_t file, shm_coordinator_t coordinator);

#ifdef __cplusplus
}
#endif

#endif

