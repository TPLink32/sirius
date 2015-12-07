#ifndef __READ_DATATAP_H
#define __READ_DATATAP_H

#include <ffs.h>
#include <atl.h>
#include <evpath.h>
#include <pthread.h>

#ifdef _NOMPI
    /* Sequential processes can use the library compiled with -D_NOMPI */
#   include "mpidummy.h"
#define MPI_SUM 0
#else
    /* Parallel applications should use MPI to communicate file info and slices of data */
#   include "mpi.h"
#endif

#if HAVE_PORTALS == 1

#include "thin_portal.h"


#define VAR_BITMAP_SIZE 16

typedef struct _datatap_var_chunk
{
    int rank;
    void *data;
    uint64_t *local_bounds; // ndims
    uint64_t *global_bounds; // ndims
    uint64_t *global_offsets; // ndims
    struct _datata_var_chunk *next;
} datatap_var_chunk, *datatap_var_chunk_p;

typedef struct _datatap_var_info
{
    int id;
    char *varname;
    char *varpath;
    enum ADIOS_DATATYPES type;
    uint64_t data_size;
    int time_dim; // -1 means no time dimension
    int ndims;
    int num_chunks;
    datatap_var_chunk *chunks;
    struct _datatap_var_info *next;
} datatap_var_info, *datatap_var_info_p;


typedef struct _datatap_pg_info
{
    int rank;
    int num_vars;
    datatap_var_info **vars;
    datatap_var_chunk **var_chunks;
} datatap_pg_info, *datatap_pg_info_p;

typedef struct _datatap_read_file_data
{
    char *file_name;
    char *group_name; // TODO: assume one group in file
    file_info *f_info;
    int num_vars;
    datatap_var_info *vars;

    // a list of vars which is not present locally
    int num_vars_peer;
    datatap_var_info *vars_peer;

    MPI_Comm comm;
    int my_rank;
    int comm_size;
    int host_language_fortran; // 1 for Fortran; 0 for C
    char var_bitmap[VAR_BITMAP_SIZE]; // 128 bit for 128 var
    datatap_pg_info *pgs; // size is f_info->num_chunks
} datatap_read_file_data, *datatap_read_file_data_p;

typedef struct _datatap_read_method_data
{
    pthread_t dt_server_thread;
    MPI_Comm dt_comm;
    int dt_comm_rank;
    int dt_comm_size;
    CManager dt_cm;
    IOhandle *dt_handle;
    int dt_server_ready;
    int num_io_dumps;    // TODO: timestep
} datatap_read_method_data, *datatap_read_method_data_p;

typedef struct _Queue_Item
{
    uint32_t length;
    char * data;
    FMStructDescList var_list;
    int32_t rank;     // rank of client from which the chunk comes
    datatap_pg_info *pg_info;
} QueueItem, pg_chunk;

// get the next PG 
pg_chunk *get_next_chunk(ADIOS_FILE *fp);

// return addres of var in chunk, var_info and var_chunk will contain
// var information such as type, dimention, offset 
void *read_var_in_chunk(pg_chunk *pg, 
                        char *varname, 
                        datatap_var_info **var_info, 
                        datatap_var_chunk **var_chunk);

// equavalent to inq_var, but only related to the specified chunk
ADIOS_VARINFO *inq_var_in_chunk(ADIOS_GROUP *gp, pg_chunk *pg, char *varname);

// return buffer for recycling
void return_chunk_buffer(ADIOS_FILE *fp, pg_chunk *chunk);

#endif

// define stream property
// order: 
// 
// int define_stream(ADIOS_FILE file, hint);

#endif
