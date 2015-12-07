/*
 * A new set of ADIOS read APIs suitable for chunk-based read in shared memory
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"
#include "adios_types.h"
#include "meta_data.h"
#include "read_shm.h"
#include "thread_pin.h"

// TODO: move default parameters to a common place
#define META_REGION_SIZE (500*1024)

// global bookkeeping data for SHM transport at reader side
shm_read_method_data_t shm_mdata = NULL;

// number of nodes this reader program is running on
int num_nodes;

/*
 * initialize SHM transport
 */
int adios_read_shm_init(MPI_Comm comm)
{
    if(!shm_mdata || !shm_mdata->initialized) {
        shm_mdata = (shm_read_method_data_t) 
            malloc(sizeof(shm_read_method_data));
        if(!shm_mdata) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
  
        // TODO: create the meta data region 
        int rank;
        int mpisize;
        MPI_Comm_rank(comm, &rank);
        MPI_Comm_size(comm, &mpisize);

        int m_region_total_size = META_REGION_SIZE;

        // TODO: mpi should have a way to retrieve number of "real" nodes 
        // on which a program runs
        get_num_nodes(&num_nodes);
        if(num_nodes == -1) {
            num_nodes = mpisize;
        }
        int num_process_per_node = mpisize / num_nodes;

        // number of maximum possible writers per file within a node
        int num_writers;
        char *temp_string = getenv("NUM_WRITERS");
        if(temp_string) {
            num_writers = atoi(temp_string);
        }
        else { // assume one process per node
            num_writers = MAX_WRITERS_PER_FILE;
        }

        // TODO: query from mpi 
        int byslot = 1;
        int is_creator;
        if(byslot) {
            is_creator = (rank % num_process_per_node == 0) ? 1:0;
        }
        else {
            is_creator = (rank / num_nodes == 0) ? 1:0;
        }
        int rc;
        if(is_creator) {
            rc = create_meta_region(m_region_total_size, num_writers);
        }
        else {
            rc = initialize_meta_region(m_region_total_size);
        }
        if(rc) {
            free(shm_mdata);
            shm_mdata = NULL;
            return rc;
        }  
        shm_mdata->is_creator = is_creator;
        shm_mdata->initialized = 1;
    }    
    return 0;
}

/*
 * finalize SHM transport
 */
int adios_read_shm_finalize()
{
    // make sure all files are closed
    if(shm_mdata->is_creator) {
        destroy_meta_region();
    }
    else {
        finalize_meta_region();
    }
    return 0;
}

/*
 * open file for read
 * this will be called only once for the first timestep
 */
shm_read_file_data *adios_read_shm_fopen(char *fname, MPI_Comm comm, int blocking)
{
    shm_read_file_data *f = (shm_read_file_data *) malloc(sizeof(shm_read_file_data));
    if(!f) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }

    f->comm = comm;
    MPI_Comm_rank(comm, &f->my_rank);
    MPI_Comm_size(comm, &f->comm_size);

    // create coordinator
    // TODO: get parameter from outside
    int num_readers_per_node = f->comm_size / num_nodes;
    int byslot = 1;
    int color;
    int key;
    if(byslot) {
        color = f->my_rank / num_readers_per_node;
        key = f->my_rank % num_readers_per_node;
    }
    else { // TODO: by node
        color = f->my_rank % num_nodes;
        key = f->my_rank / num_nodes;
    }
    MPI_Comm newcomm; // TODO: we need to put this in heap
    MPI_Comm_split(comm, color, key, &newcomm);
    f->coordinator = shm_create_coordinator(newcomm);
    if(!f->coordinator) {
        free(f);
        return NULL;
    }

    // look up meta data region for file
    // this is a blocking call
    while(1) {
        f->file = get_file_byname(fname, f->coordinator);
  
        if(!f->file) { // file is not available yet
            if(blocking) {
                sleep(1); 
                continue; 
            }
            else {
                shm_destroy_coordinator(f->coordinator); 
                free(f);
                return NULL;
            }
        }
        else {
            break;
        }
    }

    // we should have locked the file at this time
    
    // TODO: set up writer-reader mapping. Here every reader attaches all data queues
    // create data queue end point at reader side
    f->data_q = get_data_queues(f->file);

    // TODO: Redundent 
    f->file_name = f->file->file_name; 
    f->group_name = f->file->group_name;
    f->timestep = f->file->timestep;     // start from 0

    // TODO: set up writer-reader mapping. 
    f->num_pgs = f->file->num_writers;

    // TODO: we can unlock the file now, mark its state as being read
    unlock_file_by_reader(f->file, f->coordinator);

    f->pgs = (shm_pg_info *) malloc(f->num_pgs * sizeof(shm_pg_info));
    if(!f->pgs) {
        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
        return NULL;
    }

    return f;
}

/*
 * close file
 * this will be called only once when reader finishes reading the file
 */
int adios_read_shm_fclose(shm_read_file_data *f)
{
    // dettach data queues
    int rc = close_data_queues(f->data_q, f->num_pgs);

    // make sure all timesteps have been read
    rc = close_file_r(f->file, f->coordinator);

    // cleanup
    // TODO: cleanup; pgs and vars should be freed in finish_timestep()
    int i;
    for(i = 0; i < f->num_pgs; i ++) {
        if(f->pgs[i].vars) {
            free(f->pgs[i].vars);
        }
    }
    free(f->pgs);
    free(f);
    return rc;
}

/*
 * open new timestep 
 * NOTE: this function blocks until new timestep is available or EOF is reached
 * return: 0: got new timestep
 *         1: reach EOF
 *        -1: error
 */
int adios_read_shm_get_timestep(shm_read_file_data *f, int tidx)
{
    enum FILE_STATE rc = -1;
    while(1) {
        rc = get_new_timestep(f->file, tidx, f->coordinator);
        if(rc == file_state_reading) {
            f->timestep = tidx;
            break;
        }
        else if(rc == file_state_unavail) {
            sleep(1);
            continue;
        }
        else if(rc == file_state_EOF) {
            return 1;
        }
    }
    // now we have locked the file

    // for each pg, parse meta-data
    int i;
    for(i = 0; i < f->num_pgs; i ++) {

        // TODO: make sure the current slot is ready for reading
        while(!shm_q_is_full(f->data_q[i].queue));

        shm_q_slot_t slot = shm_q_get_current_slot(f->data_q[i].queue);

        f->pgs[i].data_q = &f->data_q[i];
        char *pos_meta = shm_q_get_data_in_slot(f->pgs[i].data_q->queue, slot);
        int meta_data_size = *(int *)pos_meta;
        char *pos = pos_meta + meta_data_size; // where actual data resides

        // TODO: skip redundent information
        pos_meta +=  sizeof(int)  // size of meta data
                   + sizeof(enum ADIOS_FLAG) // host language
                   + sizeof(int)  // size of group name
                   + strlen(f->group_name) + 1;  // group name string
        memcpy(&(f->pgs[i].num_vars), pos_meta, sizeof(int));
        pos_meta += sizeof(int);

        // TODO: free this later. or re-use them if possible?
        f->pgs[i].vars = (shm_var_info_t)
            malloc(f->pgs[i].num_vars * sizeof(shm_var_info));
        if(!f->pgs[i].vars) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
            return -1;
        }
        // now extract meta data and data for each variable
        int temp_int;
        int j;
        for(j = 0; j < f->pgs[i].num_vars; j ++) {
            shm_var_info_t v = &f->pgs[i].vars[j];
            memcpy(&temp_int, pos_meta, sizeof(int));  // var info size
            pos_meta += sizeof(int);
            memcpy(&v->id, pos_meta, sizeof(int)); // var id
            pos_meta += sizeof(int);
            memcpy(&temp_int, pos_meta, sizeof(int)); // var name length
            pos_meta += sizeof(int);
            v->name = pos_meta;
            pos_meta += temp_int;
            memcpy(&temp_int, pos_meta, sizeof(int)); // var path length
            pos_meta += sizeof(int);
            v->path = pos_meta;
            pos_meta += temp_int;
            memcpy(&v->type, pos_meta, sizeof(enum ADIOS_DATATYPES)); // type
            pos_meta += sizeof(enum ADIOS_DATATYPES);
            memcpy(&v->time_dim, pos_meta, sizeof(int)); // whether have time index
            pos_meta += sizeof(int);
            memcpy(&v->ndims, pos_meta, sizeof(int)); // number of dimensions
            pos_meta += sizeof(int);
            if(v->ndims) { // array
                int dim_size = 3*v->ndims*sizeof(uint64_t);
                v->dims = (uint64_t *) malloc(dim_size);
                if(!v->dims) {
                    fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
                    return -1;
                }
                memcpy(v->dims, pos_meta, dim_size);
                pos_meta += dim_size;
            }
            uint64_t addr;
            memcpy(&addr, pos_meta, sizeof(uint64_t));
            pos_meta += sizeof(uint64_t);
            v->data = shm_q_get_addr(f->data_q[i].queue, addr);
        }
    }

    return 0;
}

/*
 * test if new timestep is available
 * NOTE: obsolete
 */
int adios_read_shm_is_timestep_ready(shm_read_file_data *f, int tidx)
{
    int rc = is_new_timestep_ready(f->file, tidx, f->coordinator);
    return rc;
}

/*
 * mark current timestep of pg as been read
 */
void adios_read_shm_release_pg(shm_pg_info_t pg)
{
    free(pg->vars);
    pg->vars = NULL;
    pg->num_vars = 0;
    shm_q_release_current_slot(pg->data_q->queue);
}

/*
 * mark current timestep of whole file as been read
 */
int adios_read_shm_advance_timestep(shm_read_file_data *f)
{
    int i;
    for(i = 0; i < f->num_pgs; i ++) {
        if(f->pgs[i].vars) {
            free(f->pgs[i].vars);
        }
        f->pgs[i].vars = NULL;
        f->pgs[i].num_vars = 0;
    }

    // TODO: mark each every pg as been read

    // mark file as been read in meta data region
    // unlock the file
    int rc = finish_read_timestep(f->file, f->coordinator);

    return rc;
}

/*
 * return handle of a variable inside a pg
 */
shm_var_info_t adios_read_shm_get_var_byname(shm_pg_info *pg, char *varname)
{
    shm_var_info_t v = pg->vars;
    
    int i;
    for(i = 0; i < pg->num_vars; i ++) {
        if(!strcmp(varname, v[i].name)) {
            return &v[i];
        }
    }
    return NULL;
}

