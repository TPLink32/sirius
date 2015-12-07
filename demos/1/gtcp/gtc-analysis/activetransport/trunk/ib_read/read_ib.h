#ifndef _ADIOS_READ_IB_H_
#define _ADIOS_READ_IB_H_
/*
 * A new set of ADIOS read APIs suitable for chunk-based read using IBPBIO
 * as underlying transport
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>
#include "ffs.h"
#include "atl.h"
#include "evpath.h"
#include "adios_types.h"
#include "thin_ib.h"

#ifdef _NOMPI
/* Sequential processes can use the library compiled with -D_NOMPI */
#include "mpidummy.h"
#define MPI_SUM 0
#else
/* Parallel applications should use MPI to communicate file info and slices of data */
#include "mpi.h"
#endif

// default parameters
#define CACHE_LINE_SIZE 128
#define IS_THREADED 1
//#define MAX_FILE_NAME_LEN_IB 20
//#define MAX_GRP_NAME_LEN_IB 20
#define MAX_NUM_FILES 1
#define MAX_NUM_TIMESTEPS 1

/*
 * meta-daa of a variable
 *
 * TODO: convert from FFS
 */
typedef struct _ib_var_info
{
    int id;
    char *name;
    char *path;
    enum ADIOS_DATATYPES type;
    int time_dim;   // -1 means no time dimension
    int ndims;
    uint64_t *dims; // 3 * ndims: l:g:o
    void *data;
} ib_var_info, *ib_var_info_t;

/*
 * meta-data of a process group (i.e., chunk)
 */
typedef struct _ib_pg_info
{
    int rank;                    // rank of client which wrote this pg
    uint32_t length;             // decoded data size
    char *data;                  // decoded data
    FMStructDescList var_list;   // FFS format list
    int num_vars;                // TODO
    ib_var_info_t vars;          // TODO
} pg_info_ib, *pg_info_ib_t;

/*
 * per-timestep meta-data
 */
enum TIMESTEP_STATE_IB {timestep_state_incoming = 0, // writing
                        timestep_state_ready = 1,    // ready for reading
                        timestep_state_reading = 2,  // reading
                        timestep_state_unavail = 3,  // empty slot or been read
                        timestep_state_eof = 4       // End of File
                       };

typedef struct _file_timestep_ib {
    enum TIMESTEP_STATE_IB state;
    int tidx;
    int num_pgs;
    int num_pgs_incoming;
    pg_info_ib_t pgs;

    // for timing
    double create_time; // time receiving the first chunk
    double ready_time;  // time receiving all chunks
    double open_time;   // time opened by applicaiton thread
    double release_time; // time released by application thread

    char padding[CACHE_LINE_SIZE - sizeof(int)*3 - sizeof(enum TIMESTEP_STATE_IB) - sizeof(pg_info_ib_t)];
} file_timestep_ib, *file_timestep_ib_t;

/*
 * per-file meta-data
 */
enum FILE_STATE_IB {file_state_unavail = 0,// empty  
                    file_state_ready = 1,  // ready for reading (at least one timestep been written)
                    file_state_eof = 2,    // writer done
                    file_state_done = 3    // reader done
                   };

typedef struct _file_info_ib {
    char fname[MAX_FILE_NAME_LEN_IB];
    char gname[MAX_GRP_NAME_LEN_IB];
    
    enum FILE_STATE_IB state;   // state of file
    int num_writers;            // total number of writers for this file
    int num_pgs;                // number of pgs read by this process
    int host_language_fortran;  // 1 for Fortran; 0 for C
    int tidx_start;             // First timestep in file, usually 1
    int timestep;               // The current (only) timestep
    int ntimesteps;             // Number of timesteps in file. There is always at least one timestep
    int lasttimestep;           // The currently available latest timestep in the stream
    int max_num_timesteps;
    file_timestep_ib_t timesteps;// point to each timestep

    char padding[CACHE_LINE_SIZE - sizeof(int)*8 - MAX_FILE_NAME_LEN_IB - MAX_GRP_NAME_LEN_IB - 
                 sizeof(enum FILE_STATE_IB) - sizeof(file_timestep_ib_t)];
} file_info_ib, *file_info_ib_t;

typedef struct _ib_read_file_data
{
    file_info_ib_t f_info;
    file_timestep_ib_t timestep;   
    int max_num_timesteps;
    int current_tidx;
    MPI_Comm comm;
    int my_rank;
    int comm_size;
} ib_read_file_data, *ib_read_file_data_t;

/*
 * global meta-data region, organized as a simple static array
 */
typedef struct _file_index_ib {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int max_num_files;
    int next;
    file_info_ib_t files;
} file_index_ib, *file_index_ib_t;

/*
 * global bookkeeping data for IB transport at reader side
 */
typedef struct _ib_read_method_data
{
    int initialized;
    int is_threaded; 
    volatile int dt_server_ready;
    pthread_t dt_server_thread;
    MPI_Comm dt_comm;
    int dt_comm_rank;
    int dt_comm_size;
    CManager dt_cm;
    IOhandle *dt_handle;
    file_index_ib_t file_index;
} ib_read_method_data, *ib_read_method_data_t;

/*
 * chunk read API
 */

/*
 * initialize IB transport
 */
int adios_read_ib_init(MPI_Comm comm);

/*
 * finalize IB transport
 */
int adios_read_ib_finalize();

/*
 * open file for read
 */
ib_read_file_data *adios_read_ib_fopen(char *fname, MPI_Comm comm, int blocking);

/*
 * close file
 */
int adios_read_ib_fclose(ib_read_file_data *fp);

/*
 * open new timestep
 * NOTE: this function blocks until new timestep is available
 */
int adios_read_ib_get_timestep(ib_read_file_data *fp, int tidx);

/*
 * test if new timestep is available
 * NOTE: obsolete
 */
int adios_read_ib_is_timestep_ready(ib_read_file_data *fp, int tidx);

/*
 * mark current timestep of pg as been read
 */
void adios_read_ib_release_pg(pg_info_ib_t pg);

/*
 * return handle of a variable inside a pg
 */
void *adios_read_ib_get_var_byname(pg_info_ib_t pg, char *varname);

int get_application_id();

void set_application_id(int id);

#ifdef __cplusplus
}
#endif

#endif
 
