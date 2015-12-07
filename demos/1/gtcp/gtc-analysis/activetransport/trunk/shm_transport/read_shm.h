#ifndef _ADIOS_READ_SHM_H_
#define _ADIOS_READ_SHM_H_
/*
 * A new set of ADIOS read APIs suitable for chunk-based read in shared memory
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mpi.h"
#include "adios_types.h"
#include "meta_data.h"

/*
 * meta-daa of a variable
 */ 
typedef struct _shm_var_info
{
    int id;
    char *name;
    char *path;
    enum ADIOS_DATATYPES type;
    int time_dim; // -1 means no time dimension
    int ndims;
    uint64_t *dims; // 3 * ndims: l:g:o
    void *data;
} shm_var_info, *shm_var_info_t;

/*
 * meta-data of a process group (i.e., chunk)
 */
typedef struct _shm_pg_info
{
    shm_data_queue_r_t data_q; 
    int num_vars;
    shm_var_info_t vars;
} shm_pg_info, *shm_pg_info_t;

/*
 * per-file meta-data
 */
typedef struct _shm_read_file_data
{
    char *file_name;
    char *group_name; // TODO: assume one group in file

    int timestep;     // start from 0
    int num_pgs;
    shm_data_queue_r_t data_q; 
    shm_pg_info_t pgs; 
    shm_coordinator_t coordinator;
    shm_file_index_t file;

//    file_info *f_info;
//    // a list of vars which is not present locally
//    int num_vars_peer;
//    shm_var_info *vars_peer;
//    char var_bitmap[VAR_BITMAP_SIZE]; // 128 bit for 128 var

    MPI_Comm comm;
    int my_rank;
    int comm_size;
} shm_read_file_data, *shm_read_file_data_t;

/*
 * global bookkeeping data for SHM transport at reader side
 */
typedef struct _shm_read_method_data
{
    int is_creator; // creator of local meta-data region
    int initialized;
} shm_read_method_data, *shm_read_method_data_t;


/*
 * Chunk read API
 */

/*
 * initialize SHM transport
 */
int adios_read_shm_init(MPI_Comm comm);

/*
 * finalize SHM transport
 */
int adios_read_shm_finalize();

/*
 * open file for read
 */
shm_read_file_data *adios_read_shm_fopen(char *fname, MPI_Comm comm, int blocking);

/*
 * close file
 */
int adios_read_shm_fclose(shm_read_file_data *fp);

/*
 * open new timestep 
 * NOTE: this function blocks until new timestep is available
 */
int adios_read_shm_get_timestep(shm_read_file_data *fp, int tidx);

/*
 * test if new timestep is available
 * NOTE: obsolete
 */
int adios_read_shm_is_timestep_ready(shm_read_file_data *fp, int tidx);

/*
 * mark current timestep of pg as been read
 */
void adios_read_shm_release_pg(shm_pg_info_t pg);

/*
 * mark current timestep as been read
 */
int adios_read_shm_advance_timestep(shm_read_file_data *fp);

/*
 * return handle of a variable inside a pg
 */
shm_var_info *adios_read_shm_get_var_byname(shm_pg_info *pg, char *varname);

#ifdef __cplusplus
}
#endif

#endif

