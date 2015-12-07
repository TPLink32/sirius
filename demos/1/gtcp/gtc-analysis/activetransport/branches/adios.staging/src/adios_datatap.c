#include "config.h"

#define HAVE_PORTALS  0
//#define HAVE_INFINIBAND  0

#if NO_DATATAP == 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ffs.h>
#include <atl.h>
#include <mpi.h>
#include "adios.h"
#include "adios_internals.h"
#include "adios_transport_hooks.h"
#include "bp_utils.h"
// added by zf2 (moved getFixedname and get_full_path_name to globals.c)
#include "globals.h"

#include <sys/queue.h>
#if HAVE_PORTALS == 1
#define QUEUE_H 1
#include <thin_portal.h>
#elif HAVE_INFINIBAND == 1
#include <thin_ib.h>
#endif

/////////////#include <time.h>
/////////////#include "lgs.h"
/////////////#include "cercs_env.h"

// for SHM transport
#include "meta_data.h"
#include "thread_pin.h"

static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

//static LIST_HEAD(listhead, fm_structure) globallist;

// added by zf2: moved to globals.c
//#define OPLEN 4
//static char OP[OPLEN] = { '+', '-', '*', '/' };
//static char *OP_REP[OPLEN] = { "_plus_", "_minus_", "_mult_", "_div_" };


typedef struct nametable_
{
    char *originalname;
    char *mangledname;
    LIST_ENTRY(nametable_) entries;
} nametable;

typedef struct altname_
{
    char *name;
    FMField *field;                                               //contains the field list
    LIST_ENTRY(altname_) entries;
} altname;

typedef struct dimnames_
{
    char *name;
    LIST_HEAD(alts, altname_) altlist;
    LIST_ENTRY(dimnames_) entries;
} dimnames;

typedef struct _var_to_write
{
    struct adios_var_struct *var;
    struct _var_to_write *next;
} var_to_write;

typedef struct _vars_list
{
    var_to_write *head;
    var_to_write *tail;
} vars_list;

// for RMDA transport
struct fm_structure
{
    FMFormatRec *format;
    int size;       //in bytes - no padding
    unsigned char *buffer;
    int snd_count;  // TODO: this count for adios_close
    IOhandle *s;
    FMFormat ioformat;
    attr_list alist;    

    vars_list vars_to_write; // adios tracks this in g->vars_written
    int var_info_buffer_size;
    char *var_info_buffer;

    LIST_HEAD(tablehead, nametable_) namelist;
    LIST_HEAD(dims, dimnames_) dimlist;
};

struct dim_var{
    void *var_addr;
    void *value;
};

#define DIM_VAR_MAX 20

struct shm_structure
{
    shm_coordinator_t coordinator;
    shm_data_queue_t data_q;
    shm_q_slot_t slot;
    shm_file_index_t file;
    char *current_pos;
    char *current_pos_meta;
    int meta_data_size;
    int num_dim_vars;
    struct dim_var dims[DIM_VAR_MAX]; // TODO: make this dynamic
};

typedef struct datatap_method_data
{
    int initialized;

    // added by zf2
    enum ADIOS_FLAG use_shm_transport;

    int num_vars;            // adios tracks this already

    // for shm transport
    struct shm_structure *shm;

    // for RDMA transport
    struct fm_structure *fm;

    int opencount;  // TODO: this count for adios_open
    int cycle_id;
    char *pfile;
} dmd;

// added by zf2: rank and mpisize will be obsolete. We will use a per-group mpi comm/rank/size
MPI_Comm adios_mpi_comm_world = MPI_COMM_WORLD;
int rank = -1;
int mpisize  = 1;


static int add_var_to_list(vars_list *var_list, struct adios_var_struct *var)
{
    var_to_write *t = (var_to_write *) malloc(sizeof(var_to_write));
    if(!t) {
        fprintf(stderr, "cannot allocate memory.\n");
        return -1;
    }
    t->var = var;
    t->next= NULL;

    if(var_list->tail) {
        var_list->tail->next = t;
    }
    var_list->tail = t;
    if(!var_list->head) {
        var_list->head = t;
    }
    return 0;
}

static void reset_var_list(vars_list *var_list)
{
    var_list->head = var_list->tail = NULL;
}

static struct adios_var_struct *next_var_from_list(vars_list *var_list)
{
    struct adios_var_struct *v;
    if(!var_list->head) {
        return NULL;
    }
    else {
        var_to_write *t;
        t = var_list->head;
        v = var_list->head->var;
        var_list->head = var_list->head->next;
        free(t);
        return v;
    }
}

/*
 * return the value of a variable as an integer
 * the variable is used to specify array dimension
 */
uint64_t get_dim_var_value(struct adios_var_struct *dim_var, 
                           void *dim_value
                          ) 
{
    // get the value
    switch(dim_var->type) {
        case adios_integer:
        {
            return *((int *)dim_value);
        }
        case adios_unsigned_integer:
        {
            return *((unsigned int *)dim_value);
        }
        case adios_long:
        {
            return *((long int *)dim_value);
        }
        case adios_short:
        {
            return *((short int *)dim_value);
        }
        case adios_unsigned_short:
        {
            return *((unsigned short int *)dim_value);
        }
        case adios_unsigned_long:
        {
            return *((unsigned long int *)dim_value);
        }
        default:
        {
            fprintf(stderr, "cannot support data type %d as dimension.", dim_var->type);
            return -1;
        } 
    }
}

// added by zf2: moved to globals.c

//static char *
//getFixedName(char *name)
//{
//    char *tempname = (char *) malloc(sizeof(char) * 255);
//    tempname = strdup(name);
//
//
//    char *oldname = strdup(name);
//    char *loc = NULL;
//    int i;
//
//    do
//    {
//        for (i = 0; i < OPLEN; i++)
//        {
//    //checking operator OP[i]
//            loc = strchr(oldname, OP[i]);
//            if (loc == NULL)
//                continue;
//            *loc = 0;
//            snprintf(tempname, 255, "%s%s%s", oldname, OP_REP[i], &loc[1]);
//            free(oldname);
//            oldname = strdup(tempname);
//        }
//    }
//    while (loc != NULL);
//
//
//
//    return tempname;
//}

// added by zf2: do not use alternative dimension variable names
////return a list of all the names associated with the variable
//static char **
//getAltName(char *variable, int *count)
//{
//    if (count == NULL)
//        return NULL;
//
//    return NULL;
//
//}
//
//findAltName(struct fm_structure *current_fm, char *dimname, char *varname)
//{
//    int len = strlen(dimname) + strlen(varname) + 2;
//    char *aname = (char *) malloc(sizeof(char) * len);
//    strcpy(aname, dimname);
//    strcat(aname, "_");
//    strcat(aname, varname);
//    dimnames *d = NULL;
//
//    for (d = current_fm->dimlist.lh_first; d != NULL; d = d->entries.le_next)
//    {
//        if (!strcmp(d->name, dimname))
//        {
//            //matched
//            break;
//        }
//    }
//    if (d == NULL)
//    {
//        d = (dimnames *) malloc(sizeof(dimnames));
//
//       // added by zf2
//        //d->name = dimname;
//        d->name = strdup(dimname);
//        LIST_INIT(&d->altlist);
//        LIST_INSERT_HEAD(&current_fm->dimlist, d, entries);
//    }
//    FMField *field = (FMField *) malloc(sizeof(FMField));
//    altname *a = (altname *) malloc(sizeof(altname));
//    a->name = aname;
//    a->field = field;
//    field->field_name = strdup(aname);
//    field->field_type = strdup("integer");
//    field->field_size = sizeof(int);
//    field->field_offset = -1;
//    LIST_INSERT_HEAD(&d->altlist, a, entries);
//    return a;
//}

//static char *
//findFixedName(struct fm_structure *fm, char *name)
//{
//    nametable *node;
//    for (node = fm->namelist.lh_first; node != NULL;
//         node = node->entries.le_next)
//    {
//        if (!strcmp(node->originalname, name))
//        {
//            //matched
//            return node->mangledname;
//        }
//    }
//    return name;
//}

extern void
adios_datatap_init(const char *params, struct adios_method_struct *method)
{
    if (method->method_data != NULL)
    {
        dmd *mdata = (dmd *) method->method_data;
        if (mdata->initialized == 1)
            return;
    }

    method->method_data = (void *) malloc(sizeof(struct datatap_method_data));
    dmd *mdata = (dmd *) method->method_data;
    memset(mdata, 0, sizeof(dmd));

    mdata->opencount = 0;
    mdata->initialized = 1;
    if (params != NULL && strlen(params) >= 1) {
/////////////////////////////////////////////////////////////
        //contains the file name of the file to read?
        // added by zf2: here we use config.xml to pass reader application id
        mdata->pfile = strdup(params);
/////////////////////////////////////////////////////////////
    }
    else {
        mdata->pfile = NULL;
    }
    
    MPI_Comm_rank(adios_mpi_comm_world, &rank);
    MPI_Comm_size(adios_mpi_comm_world, &mpisize);

    // TODO: set from config.xml parameter string
    char *useshm_string = getenv("DATATAP_USE_SHM");
    if(useshm_string) {
        mdata->use_shm_transport = adios_flag_yes; // yes: shm; no: rdma
    }
    else {
        mdata->use_shm_transport = adios_flag_no; // yes: shm; no: rdma
    }
}

extern int
adios_datatap_open(struct adios_file_struct *fd,
           struct adios_method_struct *method, void*comm)
{
    if (fd == NULL || method == NULL)
    {
        fprintf(stderr, "Bad input parameters\n");
        return -1;
    }

    dmd *mdata = (dmd *) method->method_data;
    if (mdata != NULL)
    {
        if (mdata->initialized == 0)
        {
            fprintf(stderr, "method not initialized properly\n");
            return -1;
        }
    }
    else
    {
        fprintf(stderr, "method not initialized\n");
        return -1;
    }

    struct adios_group_struct *t = method->group;
    if (t == NULL)
    {
        fprintf(stderr, "group is not initialized properly\n");
        return -1;
    }

    // this is not the first timestep
    if (mdata->opencount != 0)
    {
        if(mdata->use_shm_transport == adios_flag_yes) {
            // SHM transport: acquire lock on file 

            // gain meta-data lock
            wait_for_writing(mdata->shm->file, mdata->shm->coordinator);

            // TODO: writer only needs to make sure there is enough buffer in
            // local data qeueue
            while(!shm_q_is_empty(mdata->shm->data_q->queue));

            // TODO: now current slot is free for writing
            mdata->shm->slot = shm_q_get_current_slot(mdata->shm->data_q->queue);
            mdata->shm->current_pos_meta = shm_q_get_data_in_slot(mdata->shm->data_q->queue, 
                                                                  mdata->shm->slot);
            mdata->shm->current_pos_meta +=  sizeof(int)  // size of meta data
                                           + sizeof(enum ADIOS_FLAG) // host language
                                           + sizeof(int)  // size of group name
                                           + strlen(t->name) + 1  // group name string
                                           + sizeof(int);  // number of vars written

            mdata->shm->current_pos = mdata->shm->current_pos_meta + mdata->shm->meta_data_size;
        }
        else {
            // RDMA transport
//            if(strlen(fd->name) >= MAX_FILE_NAME_LEN) {
//                fprintf(stderr, "file name exceed %d\n", MAX_FILE_NAME_LEN);
//                return -1;
//            }

#if HAVE_PORTALS == 1
            strcpy(mdata->fm->s->fname, fd->name);
            mdata->fm->s->timestep = mdata->fm->snd_count; // TODO: starts from 0
#endif
        }

        mdata->opencount ++;
        return 0;
    }

    // for the first timestep, build meta-data structure, initialize transport, etc.

    mdata->num_vars = 0;

    // TODO: determine which transport to use

    if(mdata->use_shm_transport == adios_flag_yes) {
        struct shm_structure *shm = (struct shm_structure *) 
            malloc(sizeof(struct shm_structure));
        if(!shm) {
            fprintf(stderr, "cannot allocate memory. %s:%d\n",
                __FILE__, __LINE__);
            return -1;
        }
 
        // TODO: initialize meta data region
        int total_meta_size = META_REGION_SIZE;
        int rc = initialize_meta_region(total_meta_size);
        if(rc) {
            return rc; 
        }         

        // create coordinator among peer writers on the same node
        // TODO: get parameter from outside
        int num_nodes;
        get_num_nodes(&num_nodes);
        if(num_nodes == -1) {
            num_nodes = mpisize;
        }
        int num_writers_per_node = mpisize / num_nodes;
        int byslot = 1;
        int color;
        int key;
        if(byslot) {
            color = rank / num_writers_per_node;
            key = rank % num_writers_per_node;
        }
        else { // TODO: by node
            color = rank % num_nodes;
            key = rank / num_nodes;
        }
        MPI_Comm newcomm; // TODO: we need to put this in heap
        MPI_Comm_split(adios_mpi_comm_world, color, key, &newcomm);

        shm->coordinator = shm_create_coordinator(newcomm);
        if(!shm->coordinator) {
            return -1;
        }
 
        // create local data queue
        // TODO: get parameters from config.xml 
        // TODO: move the following to adios_datatap_should_buffer
        int slot_size;
        char *temp_string = getenv("DATA_QUEUE_SLOT_SIZE");
        if(temp_string) {
            slot_size = atoi(temp_string);
        }
        else {
            slot_size = DATA_QUEUE_SLOT_SIZE;
        }
        int is_static = 1;
        int num_slots = 1;
        size_t region_size = slot_size * num_slots + (1024*1024); 

        shm->data_q = create_data_queue(fd->name,
                                        t->name,
                                        rank,
                                        region_size,
                                        is_static,
                                        num_slots,
                                        slot_size
                                       );
        if(!shm->data_q) {
            return -1;
        }
        shm->slot = shm_q_get_current_slot(shm->data_q->queue);
        shm->current_pos_meta = shm_q_get_data_in_slot(shm->data_q->queue, shm->slot);

        shm->num_dim_vars = 0;

        // go through the variable list to estimate the size of meta-data
        int num_vars;
        int meta_data_size =  sizeof(int)  // size of meta data
                            + sizeof(enum ADIOS_FLAG) // host language
                            + sizeof(int)  // size of group name
                            + strlen(t->name) + 1  // group name string
                            + sizeof(int);  // number of vars written
        shm->current_pos_meta += meta_data_size;

        struct adios_var_struct *f;
        for(f = t->vars; f != NULL; f = f->next, num_vars ++) {
            int num_dims = 0;
            struct adios_dimension_struct *dim = f->dimensions;
            for(; dim != NULL; dim = dim->next) {
                if(dim->dimension.time_index != adios_flag_yes) {
                    num_dims ++;
                }
            }
 
            if(f->type == adios_string) {
                meta_data_size +=  sizeof(int)   // size of var info
                                 + strlen(f->name) + 1
                                 + strlen(f->path) + 1
                                 + 2*sizeof(int)  // var name and path
                                 + sizeof(int) // id
                                 + sizeof(int) // type
                                 + sizeof(int) // whether hav time dim
                                 + sizeof(int) // dim = 0
                                 + sizeof(uint64_t); // data value
            } 
            else if(!num_dims) { // scalar
                meta_data_size +=  sizeof(int)   // size of var info
                                 + strlen(f->name) + 1
                                 + strlen(f->path) + 1
                                 + 2*sizeof(int)  // var name and path
                                 + sizeof(int) // id
                                 + sizeof(int) // type
                                 + sizeof(int) // whether hav time dim
                                 + sizeof(int) // dim = 0
                                 + sizeof(uint64_t); // data value
                                 //TODO: + bp_get_type_size(f->type, data); // data value
            }
            else {  // arrays
                meta_data_size +=  sizeof(int)   // size of var info
                                 + strlen(f->name) + 1
                                 + strlen(f->path) + 1
                                 + 2*sizeof(int)  // var name and path
                                 + sizeof(int) // id
                                 + sizeof(int) // type
                                 + sizeof(int) // whether hav time dim
                                 + sizeof(int); // dim = N
                                 + sizeof(uint64_t); // data value

                struct adios_dimension_struct *d;
                for(d = f->dimensions; d != NULL; d = d->next) {
                    meta_data_size += 3*sizeof(uint64_t);
                    
                    // check if there is any dimension variable
                    if(d->dimension.time_index == adios_flag_yes) {
                        continue;
                    }
                    if(d->dimension.id) { // local bound
                       struct adios_var_struct *dim_var = 
                           adios_find_var_by_id(t->vars, d->dimension.id);

                       // add to lookup table
                       int j;
                       int is_new = 1;
                       for(j = 0; j < shm->num_dim_vars; j ++) {
                           if(dim_var == shm->dims[j].var_addr) {
                               // we have added this already
                               is_new = 0;
                               break;
                           }
                       }
                       // it's new, add to lookup table
                       if(is_new) {
                           shm->dims[j].var_addr = dim_var;
                           shm->num_dim_vars ++;
                           if(shm->num_dim_vars > DIM_VAR_MAX) {
                               // TODO
                               fprintf(stderr, "hit a bug! %s:%d\n", __FILE__, __LINE__);
                           }
                       }
                    }
                    if(d->global_dimension.id) { // global bound
                       struct adios_var_struct *dim_var =
                           adios_find_var_by_id(t->vars, d->global_dimension.id);

                       // add to lookup table
                       int j;
                       int is_new = 1;
                       for(j = 0; j < shm->num_dim_vars; j ++) {
                           if(dim_var == shm->dims[j].var_addr) {
                               // we have added this already
                               is_new = 0;
                               break;
                           }
                       }
                       // it's new, add to lookup table
                       if(is_new) {
                           shm->dims[j].var_addr = dim_var;
                           shm->num_dim_vars ++;
                           if(shm->num_dim_vars > DIM_VAR_MAX) {
                               // TODO
                               fprintf(stderr, "hit a bug! %s:%d\n", __FILE__, __LINE__);
                           }
                       }
                    }
                    if(d->local_offset.id) { // offset
                       struct adios_var_struct *dim_var =
                           adios_find_var_by_id(t->vars, d->local_offset.id);

                       // add to lookup table
                       int j;
                       int is_new = 1;
                       for(j = 0; j < shm->num_dim_vars; j ++) {
                           if(dim_var == shm->dims[j].var_addr) {
                               // we have added this already
                               is_new = 0;
                               break;
                           }
                       }
                       // it's new, add to lookup table
                       if(is_new) {
                           shm->dims[j].var_addr = dim_var;
                           shm->num_dim_vars ++;
                           if(shm->num_dim_vars > DIM_VAR_MAX) {
                               // TODO
                               fprintf(stderr, "hit a bug! %s:%d\n", __FILE__, __LINE__);
                           }
                       }
                    }
                }
            }
        }
        
        // TODO: page alignment
        int padding = meta_data_size % 8;
        if(padding) meta_data_size += 8 - padding;
        shm->meta_data_size = meta_data_size;
        shm->current_pos = shm->current_pos_meta + meta_data_size;

        mdata->shm = shm;
        mdata->opencount ++;
        return 0;
    }
    
    // For RDMA transport, construct FFS meta-data for marshalling and
    // connect to datatap server
    struct adios_var_struct *fields = t->vars;
    if (fields == NULL)
    {
        fprintf(stderr, "adios vars not initalized properly in the group\n");
        return -1;
    }

    //iterate through all the types
    //create a format rec
    FMFormatRec *format = (FMFormatRec *) malloc(sizeof(FMFormatRec) * 2);
    if (format == NULL)
    {
        perror("memory allocation failed");
        return -1;
    }
    memset(format, 0, sizeof(FMFormatRec) * 2);

    struct fm_structure *current_fm =
        (struct fm_structure *) malloc(sizeof(struct fm_structure));
    if (current_fm == NULL)
    {
        perror("memory allocation failed");
        return -1;
    }
    memset(current_fm, 0, sizeof(struct fm_structure));

    // added by zf2: disable attributes
    //current_fm->alist = create_attr_list();
    //set_int_attr(current_fm->alist, attr_atom_from_string("mpisize"), mpisize);
    
    // added by zf2
    reset_var_list(&(current_fm->vars_to_write));
    current_fm->snd_count = 0; // first time 

    // added by zf2: do not use var_info buffer
//    current_fm->var_info_buffer_size = 2*sizeof(int);

    LIST_INIT(&current_fm->namelist);
    LIST_INIT(&current_fm->dimlist);

    //associate the FMFormat rec with the fm_structure
    current_fm->format = format;
    format->format_name = strdup(t->name);

    //allocate field list
    if (t->var_count == 0)
    {
        fprintf(stderr, "no variables in this group - possibly an error\n");
        return -1;

    }

    // added by zf2: do not use alternative dimension variable names
    int altvarcount = 0;

    FMFieldList field_list =
        (FMFieldList) malloc(sizeof(FMField) * (t->var_count + 1));
    if (field_list == NULL)
    {
        perror("memory allocation failed");
        return -1;
    }

    //keep count of the total number of fields
    int fieldno = 0;
    int padding = 0;

    //for each type look through all the fields
    struct adios_var_struct *f;
    for (f = t->vars; f != NULL; f = f->next, fieldno++)
    {
        // added by zf2:
        // do not use full path name
//////////////////////////////////////////////////////////////////
//        char *tempname;
//        char *full_path_name = NULL;
//        int name_changed = 0;
//        full_path_name = get_full_path_name(f->name, f->path);
//        tempname = getFixedName(full_path_name);
//  
//        nametable *namenode = (nametable *) malloc(sizeof(nametable));
//        namenode->originalname = full_path_name;
//        namenode->mangledname = strdup(tempname);
//
//        LIST_INSERT_HEAD(&current_fm->namelist, namenode, entries);
//
//        field_list[fieldno].field_name = tempname;
//////////////////////////////////////////////////////////////////
        field_list[fieldno].field_name = f->name;

        // added by zf2 
        int num_dims = 0;
        struct adios_dimension_struct *dim = f->dimensions;
        for(; dim != NULL; dim = dim->next) {
            if(dim->dimension.time_index != adios_flag_yes) {
                num_dims ++;
            }
        }
        
        if(!num_dims)
        {              
//            while(current_fm->size % 8 != 0)
//            {
//                current_fm->size ++;
//            }
            padding = current_fm->size % 8;
            if(padding) {
                current_fm->size += 8 - padding;
            }

            // scalar 
            switch (f->type)
            {
                case adios_unknown:
                    fprintf(stderr, "bad type error\n");
                    fieldno--;
                    break;

                case adios_integer:
                    field_list[fieldno].field_type = strdup("integer");
                    field_list[fieldno].field_size = sizeof(int);
                    field_list[fieldno].field_offset = current_fm->size;
                    current_fm->size += sizeof(int);
                    break;

                case adios_long:
                    field_list[fieldno].field_type = strdup("integer");
                    field_list[fieldno].field_size = sizeof(long int);
                    field_list[fieldno].field_offset = current_fm->size;
                    current_fm->size += sizeof(long int);
                    break;

                case adios_real:
                    field_list[fieldno].field_type = strdup("float");
                    field_list[fieldno].field_size = sizeof(float);
                    field_list[fieldno].field_offset = current_fm->size;
                    current_fm->size += sizeof(float);
                    break;

                case adios_string:
                    field_list[fieldno].field_type = strdup("string");
                    field_list[fieldno].field_size = sizeof(char *); 
                    field_list[fieldno].field_offset = current_fm->size;
                    current_fm->size += sizeof(unsigned char *);
                    break;

                case adios_double:
                    field_list[fieldno].field_type = strdup("float");
                    field_list[fieldno].field_size = sizeof(double);
                    field_list[fieldno].field_offset = current_fm->size;
                    current_fm->size += sizeof(double);
                    break;

                case adios_byte:
                    field_list[fieldno].field_type = strdup("char");
                    field_list[fieldno].field_size = sizeof(char);
                    field_list[fieldno].field_offset = current_fm->size;
                    current_fm->size += sizeof(char);
                    break;

                default:
                    fprintf(stderr, "unknown type error %d\n", f->type);
                    fieldno--;
                    break;
            }
        }
        else
        {
            //its a vector!
            //find out the dimensions by walking the dimension list
            struct adios_dimension_struct *d = f->dimensions;
#define DIMSIZE 1024
            char dims[DIMSIZE] = { 0 };
#define ELSIZE 256
            char el[ELSIZE] = { 0 };

            // added by zf2: here we need to handle a special case where 
            // all dimensions are number, such as a[32][32][32], in this
            // case the offset should be calculated to include the whole
            // array "in place" otherwise FFSencode will fail
            int dim_are_numbers = 1;

            //create the dimension thingy
            for (; d != NULL; d = d->next)
            {
                // added by zf2: time index 
                if(d->dimension.time_index == adios_flag_yes) {
                    snprintf(el, ELSIZE, "[1]");
                    continue;
                }
  
                //for each dimension just take the upper_bound
                if (d->dimension.id)
                {
                    //findFixedName returns the mangled name from the original name
                    struct adios_var_struct *tmp_var = adios_find_var_by_id(t->vars, d->dimension.id);

                    // added by zf2: 
                    // do not use full path name 
                    // do not use alternate names to replace dimension variables
//////////////////////////////////////////////////////////////////
//                    char *name;
//                    char *full_pathname = get_full_path_name(tmp_var->name, tmp_var->path);
//                    name = findFixedName(current_fm, full_pathname);
//                    free(full_pathname);
//                    //create the alternate name for this variable and the array its defining
//                    altname *a = findAltName(current_fm, name,
//                                 (char*)field_list[fieldno].field_name);
//                    //altname is a new variable that we need to add to the field list
//                    altvarcount++;
//                    snprintf(el, ELSIZE, "[%s]", a->name);
                    snprintf(el, ELSIZE, "[%s]", tmp_var->name);
//////////////////////////////////////////////////////////////////

                    // added by zf2:
                    dim_are_numbers = 0;
                }
                else            //its a number
                {
                    //if its a number the offset_increment will be the size of the variable*rank
                    snprintf(el, ELSIZE, "[%d]", d->dimension.rank);
                }
                strncat(dims, el, DIMSIZE);
            }
//             fprintf(stderr, "%s\n", dims);

//            while(current_fm->size % 8 != 0)
//            {
//                current_fm->size ++;                    
//            }
            padding = current_fm->size % 8;
            if(padding) {
                current_fm->size += 8 - padding;
            }

            // added by zf2: we need to put the whole static array in place
            uint64_t array_size = 1;
            if(dim_are_numbers) {
                for (d = f->dimensions; d != NULL; d = d->next) {
                    if(d->dimension.time_index == adios_flag_yes) {
                        continue;
                    }
                    array_size *= d->dimension.rank;
                }
            }

            switch (f->type)
            {
                case adios_unknown:
                    fprintf(stderr, "bad type error\n");
                    fieldno--;
                    break;

                case adios_integer:
                    field_list[fieldno].field_type =
                        (char *) malloc(sizeof(char) * 255);
                    snprintf((char *) field_list[fieldno].field_type, 255,
                         "integer%s", dims);
                    field_list[fieldno].field_size = sizeof(int);
                    field_list[fieldno].field_offset = current_fm->size;
                    
                    // added by zf2
                    if(!dim_are_numbers) {
                        current_fm->size += sizeof(void *);
                    }
                    else {
                        current_fm->size += array_size * field_list[fieldno].field_size; 
                    }
                    break;

                case adios_long:
                    field_list[fieldno].field_type =
                        (char *) malloc(sizeof(char) * 255);
                    snprintf((char *) field_list[fieldno].field_type, 255,
                         "integer%s", dims);
                    field_list[fieldno].field_size = sizeof(long int);
                    field_list[fieldno].field_offset = current_fm->size;

                    // added by zf2
                    if(!dim_are_numbers) {
                        current_fm->size += sizeof(void *);
                    }
                    else {
                        current_fm->size += array_size * field_list[fieldno].field_size;
                    }
                    break;

                case adios_real:
                    field_list[fieldno].field_type =
                        (char *) malloc(sizeof(char) * 255);
                    snprintf((char *) field_list[fieldno].field_type, 255,
                         "float%s", dims);
                    field_list[fieldno].field_size = sizeof(float);
                    field_list[fieldno].field_offset = current_fm->size;

                    if(!dim_are_numbers) {
                        current_fm->size += sizeof(void *);
                    }
                    else {
                        current_fm->size += array_size * field_list[fieldno].field_size;
                    }
                    break;

                case adios_string:
                    field_list[fieldno].field_type = strdup("string");
                    field_list[fieldno].field_size = sizeof(char *);
                    field_list[fieldno].field_offset = current_fm->size;

                    if(!dim_are_numbers) {
                        current_fm->size += sizeof(void *);
                    }
                    else {
                        current_fm->size += array_size * field_list[fieldno].field_size;
                    }
                    break;

                case adios_double:
                    field_list[fieldno].field_type =
                        (char *) malloc(sizeof(char) * 255);
                    snprintf((char *) field_list[fieldno].field_type, 255,
                         "float%s", dims);
                    field_list[fieldno].field_size = sizeof(double);
                    field_list[fieldno].field_offset = current_fm->size;

                    if(!dim_are_numbers) {
                        current_fm->size += sizeof(void *);
                    }
                    else {
                        current_fm->size += array_size * field_list[fieldno].field_size;
                    }
                    break;

                case adios_byte:
                    field_list[fieldno].field_type =
                        (char *) malloc(sizeof(char) * 255);
                    snprintf((char *) field_list[fieldno].field_type, 255, "char%s",
                         dims);
                    field_list[fieldno].field_size = sizeof(char);
                    field_list[fieldno].field_offset = current_fm->size;

                    if(!dim_are_numbers) {
                        current_fm->size += sizeof(void *);
                    }
                    else {
                        current_fm->size += array_size * field_list[fieldno].field_size;
                    }
                    break;

                default:
                    fprintf(stderr, "unknown type error %d\n", f->type);
                    fieldno--;
                    break;
            }
        }
    }

    // added by zf2: do not use alternative dimension variable names
//////////////////////////////////////////////////////////////////
//    dimnames *d = NULL;
//    field_list =
//        (FMFieldList) realloc(field_list,
//                              sizeof(FMField) * (altvarcount + t->var_count +
//                                                 1));
//
//    for (d = current_fm->dimlist.lh_first; d != NULL; d = d->entries.le_next)
//    {
//        altname *a = NULL;
//        for (a = d->altlist.lh_first; a != NULL; a = a->entries.le_next)
//        {
//            a->field->field_offset = current_fm->size;
//            current_fm->size += sizeof(int);
//            memcpy(&field_list[fieldno], a->field, sizeof(FMField));
//            fieldno++;
//        }
//    }
//////////////////////////////////////////////////////////////////

    //terminate the the fieldlist
    for (; fieldno < (t->var_count + 1+altvarcount); fieldno++)
    {
        field_list[fieldno].field_type = NULL;
        field_list[fieldno].field_name = NULL;
        field_list[fieldno].field_offset = 0;
        field_list[fieldno].field_size = 0;
    }

//dump field list to check
#if 1
    {
        FMField *f = field_list;
        int x = 0;

        for (x = 0; x < fieldno; x++)
        {
            f = &field_list[x];
            if (f == NULL || f->field_name == NULL || f->field_size == 0)
                break;

              fprintf(stderr, "rank=%d %d: %s %s %d %d\n",rank,
                      x, f->field_name, f->field_type, f->field_size,
                      f->field_offset);
        }
    }
#endif

    //associate field list
    format->field_list = field_list;
    format->struct_size = current_fm->size;

    current_fm->buffer = (unsigned char *) malloc(current_fm->size);
    memset(current_fm->buffer, 0, current_fm->size);

    // For RDMA transport, make connection with datatap server
    {

#if HAVE_PORTALS == 1
//defined(__CRAYXT_COMPUTE_LINUX_TARGET)

//added by Jianting
        char *server_contact=cercs_getenv("LGS_CONTACT");
        int appid, was_set;
        appid = globals_adios_get_application_id(&was_set);
        if(!was_set) {
            fprintf(stderr, "app id was not set %s:%d\n", __FILE__, __LINE__);
            appid=1;
        }
//      init("appname", -1, server_contact);
        char app[20];
        int serv_count;
        appid=2;
        sprintf(app, "%d\0", appid);

        // added by zf2: for now just assume we have only one reader application
        char param_file[50];
        if(mdata->pfile) { // TODO: this should be per-group based not per method
            sprintf(param_file, "datatap_param%s\0", mdata->pfile);                    
        }
        else {
            strcpy(param_file, "datap_param");         
        }
//    current_fm->s = InitIOFromFile(param_file, rank,mpisize);
//fprintf(stderr,"im here %d %s:%d\n",rank,__FILE__,__LINE__);
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);
        current_fm->s = InitIOFromServList(mdata->pfile, server_contact, rank, mpisize, &end, adios_mpi_comm_world);
        current_fm->s->rank = rank;
        current_fm->ioformat = register_data(current_fm->s, current_fm->format);
        long starttime, endtime;

        starttime = start.tv_sec * 1000000000 + start.tv_nsec;
        endtime = end.tv_sec * 1000000000 + end.tv_nsec;

        long minstarttime, maxendtime;

        MPI_Reduce(&starttime, &minstarttime, 1, MPI_LONG, MPI_MIN, 0, adios_mpi_comm_world);
        MPI_Reduce(&endtime, &maxendtime, 1, MPI_LONG, MPI_MAX, 0, adios_mpi_comm_world);
        if(rank==0)
        {
            fprintf(stderr, "appid %s minstarttime is %ld, maxendtime is %ld\n", mdata->pfile, minstarttime, maxendtime);
        }

        // added by zf2
        if(strlen(fd->name) >= MAX_FILE_NAME_LEN) {
            fprintf(stderr, "file name exceed %d\n", MAX_FILE_NAME_LEN);                    
            return -1;
        }
        strcpy(current_fm->s->fname, fd->name);
//        memcpy(current_fm->s->fname, fd->name, strlen(fd->name)); 
//        current_fm->s->fname[strlen(fd->name)] = '\0';
        current_fm->s->timestep = current_fm->snd_count; // TODO: starts from 0
    
#elif HAVE_INFINIBAND == 1

        // added by zf2
        // TODO: add fname and timestep to io_handle
        if(strlen(fd->name) >= MAX_FILE_NAME_LEN_IB) {
            fprintf(stderr, "file name exceed %d\n", MAX_FILE_NAME_LEN_IB);
            return -1;
        }

        char param_file[50];
        if(mdata->pfile) { // TODO: this should be per-group based not per method
            sprintf(param_file, "datatap_param%s\0", mdata->pfile);
        }
        else {
            strcpy(param_file, "datap_param");
        }

        // TODO: convert comm to C MPI_Comm
        current_fm->s = EVthin_ib_InitIOFile(param_file, 1, adios_mpi_comm_world);
        current_fm->ioformat =
            EVthin_ib_registerData(current_fm->s, current_fm->format);

#ifdef ENSTAGE_COD
        {
            //we can read the code from here
            char codebuffer[1024*1024];
            char readbuffer[1024*1024];
        
            char *filename = getenv("FILTER");
        
            if(filename != NULL)
            {
                FILE *codefile  = fopen(filename, "r");
                if(codefile != NULL) 
                {
                    fread(readbuffer, sizeof(char), 1024*1024, codefile);
                    fclose(codefile);
                }
        
                if(!strcmp(filename, "warp_stat.c"))
                {    
//                sprintf(codebuffer, readbuffer, 0);                        
                    set_code(current_fm->s, readbuffer);
                }
                else if(!strcmp(filename, "warp_bb.c"))
                {
//                sprintf(codebuffer, readbuffer);                        
                    set_code(current_fm->s, readbuffer);                
                }
                else if(!strcmp(filename, "warp_order.c"))
                {
                    set_code(current_fm->s, readbuffer);                                            
                }
                else if(!strcmp(filename, "warp_tag.c"))
                {
                    set_code(current_fm->s, readbuffer);                                            
                }
                else if(!strcmp(filename, "warp_bbs.c"))
                {
                    set_code(current_fm->s, readbuffer);                                            
                }
            }
        }
#endif

#endif
        mdata->fm = current_fm;
        mdata->opencount ++;
    }
    return 0;
}

static FMField *
internal_find_field(char *name, FMFieldList flist)
{
    FMField *f = flist;
    while (f->field_name != NULL && strcmp(f->field_name, name))
    {
        f++;
    }
    return f;
}
 
/*
 * This function is called by adios_group_size(). Return no
 * so adios does not malloc buffer internally
 */
extern enum ADIOS_FLAG adios_datatap_should_buffer (struct adios_file_struct * fd
                                                   ,struct adios_method_struct * method)
{
    return adios_flag_no;
}

extern void
adios_datatap_write(struct adios_file_struct *fd,
                    struct adios_var_struct *f,
                    void *data, struct adios_method_struct *method)
{

//unsigned long long write_start, write_end, memcpy_start, memcpy_end, meta_start, meta_end;
//write_start = rdtsc();

    dmd *mdata = (dmd *) method->method_data;
    struct adios_group_struct *group = method->group;

    // SHM transport
    if(mdata->use_shm_transport == adios_flag_yes) {
        // get the location to write in data queue
        char *pos = mdata->shm->current_pos;
        // padding
        uint64_t padding = (uint64_t)pos % 8;
        if(padding) {
            pos += (8 - padding);
        }

        // copy data into shm queue slot         
        int num_dims = 0;
        int total_dims = 0;
        struct adios_dimension_struct *dim = f->dimensions;
        for(; dim != NULL; dim = dim->next) {
            total_dims ++;
            if(dim->dimension.time_index != adios_flag_yes) {
                num_dims ++;
            }
        }

#if 0
        if(f->type == adios_string) { // string
            if(data) {
                // copy data
                int string_size = bp_get_type_size(f->type, data);

                // TODO: check if exceed slot bound
                memcpy(pos, data, string_size);
                mdata->shm->current_pos = pos+string_size;

                // add meta-data
                char *current_pos = mdata->shm->current_pos_meta;
                int var_name_len = strlen(f->name) + 1;
                int var_path_len = strlen(f->path) + 1;
                int var_info_size = sizeof(int) * 7 
                                  + var_name_len 
                                  + var_path_len
                                  + sizeof(uint64_t);
                memcpy(current_pos, &var_info_size, sizeof(int)); // size of var info
                current_pos += sizeof(int);
                int temp_int = (int) f->id;
                memcpy(current_pos, &(temp_int), sizeof(int)); // var id
                current_pos += sizeof(int);
                memcpy(current_pos, &var_name_len, sizeof(int)); // length of var name
                current_pos += sizeof(int);
                memcpy(current_pos, f->name, var_name_len); // var name
                current_pos += var_name_len;
                memcpy(current_pos, &var_path_len, sizeof(int)); // length of var path
                current_pos += sizeof(int);
                memcpy(current_pos, f->path, var_path_len); // var path
                current_pos += var_path_len;
                memcpy(current_pos, &(f->type), sizeof(enum ADIOS_DATATYPES)); // type
                current_pos += sizeof(enum ADIOS_DATATYPES);

                if(f->dimensions && f->dimensions->dimension.time_index == adios_flag_yes) {
                     temp_int = 0; // have time dimension
                }
                else {
                     temp_int = -1; // do not have time dimension
                }
                memcpy(current_pos, &(temp_int), sizeof(int)); // whether has time index
                current_pos += sizeof(int);
                temp_int = 0;
                memcpy(current_pos, &(temp_int), sizeof(int)); // # of dimension = 0
                current_pos += sizeof(int);
                uint64_t addr = shm_q_get_offset(mdata->shm->data_q->queue, pos);
                memcpy(current_pos, &addr, sizeof(uint64_t));
                current_pos += sizeof(uint64_t);   
                mdata->shm->current_pos_meta = current_pos;
                mdata->num_vars ++;
            }
            else {
                fprintf(stderr, "no data for string %s\n", f->name);
            }
        }
        else 
#endif

        if(!num_dims) { // scalar
            if(data) {
                int data_size = bp_get_type_size(f->type, data);

//memcpy_start = rdtsc();

                // TODO: check if exceed slot bound
                memcpy(pos, data, data_size);
                mdata->shm->current_pos = pos+data_size;

//memcpy_end = rdtsc();

//meta_start = rdtsc();

                // check if this scalar is used to specify dimension
                int j;
                for(j = 0; j < mdata->shm->num_dim_vars; j ++) {
                    if(mdata->shm->dims[j].var_addr == f) {
                        mdata->shm->dims[j].value = pos;
                        break;
                    } 
                }

                // add meta-data
                char *current_pos = mdata->shm->current_pos_meta;
                int var_name_len = strlen(f->name) + 1;
                int var_path_len = strlen(f->path) + 1;
                int var_info_size = sizeof(int) * 7
                                  + var_name_len
                                  + var_path_len
                                  + sizeof(uint64_t);
                memcpy(current_pos, &var_info_size, sizeof(int)); // size of var info
                current_pos += sizeof(int);
                int temp_int = (int) f->id;
                memcpy(current_pos, &(temp_int), sizeof(int)); // var id
                current_pos += sizeof(int);
                memcpy(current_pos, &var_name_len, sizeof(int)); // length of var name
                current_pos += sizeof(int);
                memcpy(current_pos, f->name, var_name_len); // var name
                current_pos += var_name_len;
                memcpy(current_pos, &var_path_len, sizeof(int)); // length of var path
                current_pos += sizeof(int);
                memcpy(current_pos, f->path, var_path_len); // var path
                current_pos += var_path_len;
                memcpy(current_pos, &(f->type), sizeof(enum ADIOS_DATATYPES)); // type
                current_pos += sizeof(enum ADIOS_DATATYPES);

                if(f->dimensions && f->dimensions->dimension.time_index == adios_flag_yes) {
                     temp_int = 0; // have time dimension
                }
                else {
                     temp_int = -1; // do not have time dimension
                }
                memcpy(current_pos, &(temp_int), sizeof(int)); // whether has time index
                current_pos += sizeof(int);
                temp_int = 0;
                memcpy(current_pos, &(temp_int), sizeof(int)); // # of dimension = 0
                current_pos += sizeof(int);
                shm_q_handle_t q = mdata->shm->data_q->queue;
//                uint64_t addr = shm_q_get_offset(q, (void *)pos);
                uint64_t addr = ((uint64_t)pos - (uint64_t)q->mm->region->base_addr);
                memcpy(current_pos, &addr, sizeof(uint64_t));
                current_pos += sizeof(uint64_t);
                mdata->shm->current_pos_meta = current_pos;
                mdata->num_vars ++;

//meta_end = rdtsc();

 
            }
            else {
                fprintf(stderr, "no data for %s\n", f->name);
            }
        }
        else { // array
            if(data) {

//meta_start = rdtsc();

                // add meta-data
                char *current_pos = mdata->shm->current_pos_meta;
                int var_name_len = strlen(f->name) + 1;
                int var_path_len = strlen(f->path) + 1;
                int var_info_size = sizeof(int) * 7
                                  + var_name_len
                                  + var_path_len
                                  + sizeof(uint64_t)
                                  + total_dims * 3 * sizeof(uint64_t);
                memcpy(current_pos, &var_info_size, sizeof(int)); // size of var info
                current_pos += sizeof(int);
                int temp_int = (int) f->id;
                memcpy(current_pos, &(temp_int), sizeof(int)); // var id
                current_pos += sizeof(int);
                memcpy(current_pos, &var_name_len, sizeof(int)); // length of var name
                current_pos += sizeof(int);
                memcpy(current_pos, f->name, var_name_len); // var name
                current_pos += var_name_len;
                memcpy(current_pos, &var_path_len, sizeof(int)); // length of var path
                current_pos += sizeof(int);
                memcpy(current_pos, f->path, var_path_len); // var path
                current_pos += var_path_len;
                memcpy(current_pos, &(f->type), sizeof(enum ADIOS_DATATYPES)); // type
                current_pos += sizeof(enum ADIOS_DATATYPES);
                char *time_index = current_pos; // hold this to fill in later
                current_pos += sizeof(int);
                memcpy(current_pos, &total_dims, sizeof(int));
                current_pos += sizeof(int);

                uint64_t array_size = bp_get_type_size(f->type, data);
                struct adios_dimension_struct *d = f->dimensions;
                uint64_t dimensions[3]; // local bound, global bound, global offset
                int is_timeindex = 0;
                int k = 0;
                int j;
                for(; d != NULL; d = d->next, k ++) {
                    // local bound
                    if(d->dimension.time_index == adios_flag_yes) {
                        is_timeindex = 1;
                        memcpy(time_index, &k, sizeof(int));
                        dimensions[0] = 1;
                        dimensions[1] = 1;
                        dimensions[2] = group->time_index - 1;
                    }
                    else if (d->dimension.id) {
                        struct adios_var_struct *dim_var = 
                            adios_find_var_by_id(group->vars, d->dimension.id);

                        // TODO: optimize this
                        void *dim_value;
                        for(j = 0; j < mdata->shm->num_dim_vars; j ++) {
                            if(mdata->shm->dims[j].var_addr == dim_var) {
                                dim_value = mdata->shm->dims[j].value;
                                break;
                            }
                        }

                        // get the value
                        dimensions[0] = get_dim_var_value(dim_var, dim_value);
                        array_size *= dimensions[0];
                    }
                    else {   //its a number
                        array_size *= d->dimension.rank;
                    }

                    // global bounds
                    if(!is_timeindex) { // time index dimension
                        if (d->global_dimension.id) {
                            struct adios_var_struct *dim_var =
                                adios_find_var_by_id(group->vars, d->global_dimension.id);

                            // TODO: optimize this
                            void *dim_value;
                            for(j = 0; j < mdata->shm->num_dim_vars; j ++) {
                                if(mdata->shm->dims[j].var_addr == dim_var) {
                                    dim_value = mdata->shm->dims[j].value;
                                    break;
                                }
                            }

                            // get the value
                            dimensions[1] = get_dim_var_value(dim_var, dim_value);
                        }
                        else {
                            dimensions[1] = d->global_dimension.rank; 
                        }
                    }

                    // global offset
                    if(!is_timeindex) {
                        if (d->local_offset.id) {
                            struct adios_var_struct *dim_var = 
                                adios_find_var_by_id(group->vars, d->local_offset.id);

                            // TODO: optimize this
                            void *dim_value;
                            for(j = 0; j < mdata->shm->num_dim_vars; j ++) {
                                if(mdata->shm->dims[j].var_addr == dim_var) {
                                    dim_value = mdata->shm->dims[j].value;
                                    break;
                                }
                            }

                            // get the value
                            dimensions[2] = get_dim_var_value(dim_var, dim_value);
                        }
                        else {
                            dimensions[2] = d->local_offset.rank;
                        }
                    }  

                    // copy dimension info
                    memcpy(current_pos, dimensions, 3*sizeof(uint64_t));
                    current_pos += 3*sizeof(uint64_t);
                }

//meta_end = rdtsc();

//memcpy_start = rdtsc();

                // copy data
                // TODO: check if exceed slot bound
                memcpy(pos, data, array_size);
                mdata->shm->current_pos = pos + array_size;

//memcpy_end = rdtsc();

//                uint64_t addr = shm_q_get_offset(mdata->shm->data_q->queue, pos);
                uint64_t addr = ((uint64_t)pos - (uint64_t)mdata->shm->data_q->queue->mm->region->base_addr);
                memcpy(current_pos, &addr, sizeof(uint64_t));
                current_pos += sizeof(uint64_t);
                mdata->shm->current_pos_meta = current_pos;
                mdata->num_vars ++;
            }
            else {
                fprintf(stderr, "no data for %s\n", f->name);
            }
        }


//write_end = rdtsc();
//fprintf(stderr, "timing rank=%d start %lu meta %lu %lu memcpy %lu %lu end %lu\n", rank, write_start, meta_end, meta_start, memcpy_end, memcpy_start, write_end);

        return;
    }

    // RDMA transport
    struct fm_structure *fm;
    fm = mdata->fm;
 
    if (group == NULL || fm == NULL)
    {
        fprintf(stderr, "group or fm is null - improperly initialized\n");
        return;
    }
    FMFieldList flist = fm->format->field_list;
    FMField *field = NULL;

    // added by zf2
    // do not use full path name
////////////////////////////////////////////////////////
//    char *fixedname;
//    char *full_path_name = get_full_path_name(f->name, f->path);
//
//    fixedname = findFixedName(fm, full_path_name);
//    free(full_path_name);
//
//    field = internal_find_field(fixedname, flist);
////////////////////////////////////////////////////////
    field = internal_find_field(f->name, flist);

    if (field != NULL)
    {
        // added by zf2
        int num_dims = 0;
        struct adios_dimension_struct *dim = f->dimensions;
        for(; dim != NULL; dim = dim->next) {
            if(dim->dimension.time_index != adios_flag_yes) {
                num_dims ++;
            }
        }

        // string should be treat differently
        if(!strcmp(field->field_type, "string")) {
            if(data) {
                int array_size = bp_get_type_size(f->type, data);

#if 1
                //we just need to copy the pointer stored in f->data
                memcpy(&fm->buffer[field->field_offset], &data, sizeof(void *));
#else
                // allocate a temp buffer
                void *temp_buffer = malloc(array_size);
                if(!temp_buffer) {
                    fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);
                    return;
                }

                // copy data
                memcpy(temp_buffer, data, array_size);

                // record the buffer address
                memcpy(&fm->buffer[field->field_offset], &temp_buffer, sizeof(void *));
#endif

                // added by zf2
                add_var_to_list(&(fm->vars_to_write), f);
                mdata->num_vars ++;

                // added by zf2: do not use var_info buffer
/////////////////////////////////////////////////////////////////////////////////
//                fm->var_info_buffer_size += sizeof(int); // size of var info
//                fm->var_info_buffer_size += strlen(f->name) +
//                    strlen(f->path) + 2 + 2*sizeof(int);  // var name and path
//                fm->var_info_buffer_size += sizeof(int); // id
//                fm->var_info_buffer_size += sizeof(int); // type
//                fm->var_info_buffer_size += sizeof(int); // whether have time dim
//                fm->var_info_buffer_size += sizeof(int); // dim = 0
//                fm->var_info_buffer_size += array_size; // data value
//                //fm->var_info_buffer_size += 24; // TODO: let's just support plain string now
/////////////////////////////////////////////////////////////////////////////////
            }
            else {
                fprintf(stderr, "no data for string %s\n", f->name);
            }
        }
        else if (!num_dims)
        {
            //scalar quantity
            if (data)
            {
                //why wouldn't it have data?

                memcpy(&fm->buffer[field->field_offset], data,
                       field->field_size);
                // added by zf2
                add_var_to_list(&(fm->vars_to_write), f); 
                mdata->num_vars ++;

                // added by zf2: do not use var_info buffer
/////////////////////////////////////////////////////////////////////////////////
//                fm->var_info_buffer_size += sizeof(int); // size of var info
//                fm->var_info_buffer_size += strlen(f->name) +
//                    strlen(f->path) + 2 + 2*sizeof(int);  // var name and path
//                fm->var_info_buffer_size += sizeof(int); // id
//                fm->var_info_buffer_size += sizeof(int); // type
//                fm->var_info_buffer_size += sizeof(int); // whether have time dim
//                fm->var_info_buffer_size += sizeof(int); // dim = 0
//                fm->var_info_buffer_size += bp_get_type_size(f->type, data); // data value
/////////////////////////////////////////////////////////////////////////////////

                // added by zf2: do not use alternative diemension variable names
/////////////////////////////////////////////////////////////////////////////////
//                //scalar quantities can have altnames also so assign those
//                if(field->field_name != NULL)
//                {
//                    dimnames *d = NULL;
//                    for (d = fm->dimlist.lh_first; d != NULL;
//                         d = d->entries.le_next)
//                    {
//                        if (!strcmp(d->name, field->field_name))
//                        {
//                            //matches
//                            //check if there are altnames
//                            altname *a = NULL;
//                            for (a = d->altlist.lh_first; a != NULL;
//                                 a = a->entries.le_next)
//                            {
//                                //use the altname field to get the data into the buffer
//                                memcpy(&fm->buffer[a->field->field_offset], data,
//                                       a->field->field_size);
//                                //debug
//                                //int *testingint = (int*)&fm->buffer[a->field->field_offset];
//                                
//                                //   fprintf(stderr, "writing %s to %s at %d %d\n",
//                                //           f->name, a->name, a->field->field_offset,
//                                //          (int)*testingint);
//
//                            }
//                            break;
//                        }
//                    }
//                }
/////////////////////////////////////////////////////////////////////////////////
            }

            else
            {
                fprintf(stderr, "no data for scalar %s\n", f->name);
            }
        }
        else
        {
            //vector quantity
            if (data)
            {

// added by zf2: for GTS, no need to worry about temporary Fortran array or static arrays
#if 1          
                //we just need to copy the pointer stored in f->data
                memcpy(&fm->buffer[field->field_offset], &data, sizeof(void *));
#else 
                // first figure out the dimension values 
                uint64_t array_size = bp_get_type_size(f->type, data);
                struct adios_dimension_struct *d = f->dimensions;
                
                // also we need to determine if this array is static or dynamic 
                int dim_are_numbers = 1;

                for(; d != NULL; d = d->next) {
                    if(d->dimension.time_index == adios_flag_yes) {
                        continue;
                    }

                    //for each dimension just take the upper_bound
                    if (d->dimension.id) {
                        // it's dynamic array
                        dim_are_numbers = 0;                        

                        struct adios_var_struct *dim_var = adios_find_var_by_id(group->vars, d->dimension.id);

// added by zf2: do not use full path name
////////////////////////////////////////////////////////////////////////////////////////////////
//                        char *full_pathname = get_full_path_name(dim_var->name, dim_var->path);
//                        char *dimname = getFixedName(full_pathname); 
//                        free(full_pathname);
//                        FMField *dim_field = internal_find_field(dimname, flist);
////////////////////////////////////////////////////////////////////////////////////////////////
                         
                        // get the value
                        switch(dim_var->type) {
                            case adios_short:
                            {
                                //short int *temp = (short int *)(&fm->buffer[dim_field->field_offset]);
                                //array_size *= *(temp);
                                short int temp;
                                memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_field->field_size);
                                array_size *= temp;
                                break;
                            }
                            case adios_unsigned_short:
                            {
                                unsigned short int temp;
                                memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_field->field_size);
                                array_size *= temp;
                                break;
                            }
                            case adios_integer:
                            {
                                int temp;
                                memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_field->field_size);
                                array_size *= temp;
                                break;
                            }
                            case adios_unsigned_integer:
                            {
                                unsigned int temp;
                                memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_field->field_size);
                                array_size *= temp;
                                break;
                            }
                            case adios_long:
                            {
                                long int temp;
                                memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_field->field_size);
                                array_size *= temp;
                                break;
                            }
                            case adios_unsigned_long:
                            {
                                unsigned long int temp;
                                memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_field->field_size);
                                array_size *= temp;
                                break;
                            }
                            default:
                                fprintf(stderr, "cannot support data type %d as dimension.", dim_var->type);
                        }

// addeb by zf2: do not use full path name 
////////////////////////////////////////////////////////////////////////////////////////////////
//                        free(dimname);
////////////////////////////////////////////////////////////////////////////////////////////////
                    }
                    else {   //its a number
                        array_size *= d->dimension.rank;
                    }
                }

////////////////////////////////////////////////////////////////////////////////////////////////
                if(dim_are_numbers) { // copy statis array in place
                    memcpy(&fm->buffer[field->field_offset], data, array_size);
                } else {
                    // allocate a temp buffer   
                    void *temp_buffer = malloc(array_size);      
                    if(!temp_buffer) {
                        fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__, __LINE__);  
                        return; 
                    }

                    // copy data
                    memcpy(temp_buffer, data, array_size);

                    // record the buffer address
                    memcpy(&fm->buffer[field->field_offset], &temp_buffer, sizeof(void *));
                } 
////////////////////////////////////////////////////////////////////////////////////////////////
#endif
                // added by zf2
                add_var_to_list(&(fm->vars_to_write), f); 
                mdata->num_vars ++;

                // added by zf2: do not use var_info buffer
////////////////////////////////////////////////////////////////////////////////////////////////
//                fm->var_info_buffer_size += sizeof(int); // size of var info
//                fm->var_info_buffer_size += strlen(f->name) + 
//                    strlen(f->path) + 2 + 2*sizeof(int);  // var name and path
//                fm->var_info_buffer_size += sizeof(int); // id
//                fm->var_info_buffer_size += sizeof(int); // type
//                fm->var_info_buffer_size += sizeof(int); // whether hav time dim
//                fm->var_info_buffer_size += sizeof(int); // dim = N
//                for(d = f->dimensions; d != NULL; d = d->next) {
//                    fm->var_info_buffer_size += 3*sizeof(uint64_t);
//                }                                                    
////////////////////////////////////////////////////////////////////////////////////////////////
            }
            else
            {
                fprintf(stderr, "no data for vector %s\n", f->name);
            }
        }
    }


}

static void internal_adios_datatap_write(struct adios_file_struct *fd,
                                         struct adios_method_struct *method);

extern void
adios_datatap_close(struct adios_file_struct *fd,
                    struct adios_method_struct *method)
{
    dmd *mdata = method->method_data;


    if (!mdata->initialized)
    {
        return;
    }

    // added by zf2: support both write (a new file name)
    // and append (same file name, new timestep)
    if ((fd->mode & adios_mode_write) || (fd->mode & adios_mode_append))
    {
        internal_adios_datatap_write(fd, method);
    }
}

// added by zf2: do not use var_info buffer
char * create_var_info_buffer(struct adios_group_struct *g, struct fm_structure *fm, int num_vars)
{

    // format of binpacked meta-data
    // 4: total size
    // 4: language
    // 4: size of group name
    // K: group name
    // 4: num of vars
    // 4: size of var info
    // 2: var id
    // 4: size of var name
    // N: var name 
    // 4: size of var path
    // M: var path
    // 4: var type
    // 4: -1: no time dimension; otherwise specify which dimension is time dimension
    // 4: number of dimensions
    // 24: local bounds/global bounds/global offset for 1st dim
    // 24: local bounds/global bounds/global offset for 2nd dim
    // ...
    // 24: local bounds/global bounds/global offset for last dim
    
    //struct adios_var_struct *vars = g->vars;    
    fm->var_info_buffer_size += sizeof(enum ADIOS_FLAG); // host language
    fm->var_info_buffer_size += sizeof(int); // size of group name
    fm->var_info_buffer_size += strlen(g->name) + 1;

    // TODO: alignment
    char *buf = (char *) malloc(fm->var_info_buffer_size);
    if(!buf) {
        fprintf(stderr, "Cannot allocate memory. %s:%d\n", __FILE__, __LINE__);      
        exit(-1); 
    }
    
    char *current_pos = buf;    
    memcpy(current_pos, &(fm->var_info_buffer_size), sizeof(int)); // total size
    current_pos += sizeof(int);

    memcpy(current_pos, &(g->adios_host_language_fortran), sizeof(enum ADIOS_FLAG)); // host language
    current_pos += sizeof(enum ADIOS_FLAG);

    int group_name_len = strlen(g->name) + 1; 
    memcpy(current_pos, &group_name_len, sizeof(int)); // size of group name   
    current_pos += sizeof(int);
    memcpy(current_pos, g->name, group_name_len);
    current_pos += group_name_len;

    memcpy(current_pos, &(num_vars), sizeof(int)); // total num of vars
    current_pos += sizeof(int);


    FMFieldList flist = fm->format->field_list;
    struct adios_var_struct *f;
    FMField *field = NULL;
    int var_info_size;
    int var_name_len, var_path_len;
    int var_type;
    
    while(f=next_var_from_list(&(fm->vars_to_write))) {
// added by zf2: do not use full path name
////////////////////////////////////////////////////////////////
//        char *fixedname;
//        char *full_path_name = get_full_path_name(f->name, f->path);
//        fixedname = findFixedName(fm, full_path_name);
//        free(full_path_name);
//        field = internal_find_field(fixedname, flist);
////////////////////////////////////////////////////////////////
        field = internal_find_field(f->name, flist);

        var_name_len = strlen(f->name) + 1;
        var_path_len = strlen(f->path) + 1;

        int ndims = 0;
        struct adios_dimension_struct *dim = f->dimensions;
        for(; dim != NULL; dim = dim->next) {
            if(dim->dimension.time_index != adios_flag_yes) {
                ndims ++;
            }
        }

        if(!ndims) { // scalars and strings           
            var_info_size = sizeof(int) + sizeof(int) + sizeof(int) * 2 + var_name_len + var_path_len
                + sizeof(enum ADIOS_DATATYPES) + sizeof(int) *2 + field->field_size;    
            memcpy(current_pos, &var_info_size, sizeof(int)); // size of var info
            current_pos += sizeof(int);
            int id = (int) f->id;
            memcpy(current_pos, &(id), sizeof(int)); // var id
            current_pos += sizeof(int);
            memcpy(current_pos, &var_name_len, sizeof(int)); // length of var name
            current_pos += sizeof(int);
            memcpy(current_pos, f->name, var_name_len); // var name
            current_pos += var_name_len;
            memcpy(current_pos, &var_path_len, sizeof(int)); // length of var path
            current_pos += sizeof(int);
            memcpy(current_pos, f->path, var_path_len); // var path
            current_pos += var_path_len;
            memcpy(current_pos, &(f->type), sizeof(enum ADIOS_DATATYPES)); // type
            current_pos += sizeof(enum ADIOS_DATATYPES);

            if(f->dimensions && f->dimensions->dimension.time_index == adios_flag_yes) {
                 *((int *)current_pos) = 0; // have time dimension
            }
            else {
                 *((int *)current_pos) = -1; // do not have time dimension
            }
            current_pos += sizeof(int);

            *((int *)current_pos) = 0; // no of dimensions
            current_pos += sizeof(int);

            if(f->type == adios_string) {
                char *str_data = *(char **)(&fm->buffer[field->field_offset]);
                memcpy(current_pos, str_data, strlen(str_data)+1); // data value
                current_pos += strlen(str_data)+1;
            } 
            else {
                memcpy(current_pos, &fm->buffer[field->field_offset], field->field_size); // data value
                current_pos += field->field_size; 
            }
        }
        else { // arrays
            var_info_size = sizeof(int) + sizeof(int) + sizeof(int) * 2 + var_name_len + var_path_len
                + sizeof(enum ADIOS_DATATYPES) + sizeof(int) * 2; // we don't know ndims yet
            char *start_pos = current_pos;    
            //memcpy(current_pos, &var_info_size, sizeof(int)); // size of var info
            current_pos += sizeof(int);
            int id = (int) f->id;
            memcpy(current_pos, &(id), sizeof(int)); // var id
            current_pos += sizeof(int);
            memcpy(current_pos, &var_name_len, sizeof(int)); // length of var name
            current_pos += sizeof(int);
            memcpy(current_pos, f->name, var_name_len); // var name
            current_pos += var_name_len;
            memcpy(current_pos, &var_path_len, sizeof(int)); // length of var path
            current_pos += sizeof(int);
            memcpy(current_pos, f->path, var_path_len); // var path
            current_pos += var_path_len;
            memcpy(current_pos, &(f->type), sizeof(enum ADIOS_DATATYPES)); // type
            current_pos += sizeof(enum ADIOS_DATATYPES);   
        
            int *time_index = (int *) current_pos; // hold this to fill in later
            *time_index = -1; // -1 means no time dimension 
            current_pos += sizeof(int);

            int *no_dim_pos = (int *) current_pos; // hold this to fill in later
            current_pos += sizeof(int);

            uint64_t local_bound, global_bound, global_offset;
            int num_dims = 0;
            struct adios_dimension_struct *d = f->dimensions;
            for(; d != NULL; d = d->next) {
                num_dims ++;
                
                int is_timeindex = 0; 
                // local bounds
                if (d->dimension.time_index == adios_flag_yes) { // time index dimension
                    local_bound = 1; 
                    global_bound = 1; // TODO: we don't know how many timesteps in total so set to 1
                    global_offset = g->time_index - 1;
                    is_timeindex = 1;

                    // TODO: for fortran index starts from 1; for C index starts from 0 
                    *time_index = num_dims;
                }
                else if (d->dimension.id) {
                
                    // findFixedName returns the mangled name from the original name
                    struct adios_var_struct *dim_var = adios_find_var_by_id(g->vars, d->dimension.id);

                    // added by zf2: do not use full path name
/////////////////////////////////////////////////////////////////////////
//                    char *dim_name;
//                    char *full_path_name = get_full_path_name(dim_var->name, dim_var->path);
//                    dim_name = findFixedName(fm, full_path_name);
//                    free(full_path_name);
/////////////////////////////////////////////////////////////////////////
                    char *dim_name = dim_var->name;
                     
                    int dim_size = bp_get_type_size(dim_var->type, NULL);
                    
                    // get the value of it                              
                    FMField *dim_field = internal_find_field(dim_name, flist);

//                    free(dim_name);

                    // TODO: cast data type
                    switch(dim_var->type) {
                        case adios_short:
                        {
                            short int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            local_bound = (uint64_t) temp; 
                            break;
                        }
                        case adios_unsigned_short:
                        {
                            unsigned short int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            local_bound = (uint64_t) temp; 
                            break;
                        }
                        case adios_integer:
                        {
                            int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            local_bound = (uint64_t) temp; 
                            break;
                        }  
                        case adios_unsigned_integer:
                        {
                            unsigned int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            local_bound = (uint64_t) temp; 
                            break;
                        }
                        case adios_long:
                        {
                            long int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            local_bound = (uint64_t) temp; 
                            break;
                        } 
                        case adios_unsigned_long:
                        {
                            unsigned long int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            local_bound = (uint64_t) temp; 
                            break;
                        }
                        default:
                            fprintf(stderr, "cannot support data type %d as dimension.", dim_var->type);    
                    }

                }
                else { //its a number
                    local_bound = d->dimension.rank;
                }                                  

                // global bounds
                if (is_timeindex) { // time index dimension
                    global_bound = 1; // TODO: we don't know how many timesteps in total so set to 1
                }
                else if (d->global_dimension.id) {
                
                    // findFixedName returns the mangled name from the original name
                    struct adios_var_struct *dim_var = adios_find_var_by_id(g->vars, d->global_dimension.id);

                    // added by zf2: do not use full path name
/////////////////////////////////////////////////////////
//                    char *dim_name;
//                    char *full_path_name = get_full_path_name(dim_var->name, dim_var->path);
//                    dim_name = findFixedName(fm, full_path_name);
//                    free(full_path_name);
/////////////////////////////////////////////////////////
                    char *dim_name = dim_var->name;

                    int dim_size = bp_get_type_size(dim_var->type, NULL);
                    
                    // get the value of it                              
                    FMField *dim_field = internal_find_field(dim_name, flist);

//                    free(dim_name);

                    // TODO: cast data type
                    switch(dim_var->type) {
                        case adios_short:
                        {
                            short int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_bound = (uint64_t) temp; 
                            break;
                        } 
                        case adios_unsigned_short:
                        {
                            unsigned short temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_bound = (uint64_t) temp; 
                            break;
                        } 
                        case adios_integer:
                        {
                            int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_bound = (uint64_t) temp; 
                            break;
                        } 
                        case adios_unsigned_integer:
                        {
                            unsigned int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_bound = (uint64_t) temp; 
                            break;
                        } 
                        case adios_long:
                        {
                            long int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_bound = (uint64_t) temp; 
                            break;
                        } 
                        case adios_unsigned_long:
                        {
                            unsigned long int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_bound = (uint64_t) temp; 
                            break;
                        } 
                        default:
                            fprintf(stderr, "cannot support data type %d as dimension.", dim_var->type);    
                    }

                }
                else { //its a number
                    global_bound = d->global_dimension.rank;
                }                                  

                // global offset
                if (is_timeindex) { // time index dimension
                    global_offset = g->time_index - 1; 
                }
                else if (d->local_offset.id) {

                    // findFixedName returns the mangled name from the original name
                    struct adios_var_struct *dim_var = adios_find_var_by_id(g->vars, d->local_offset.id);

                    // added by zf2: do not use full path name
/////////////////////////////////////////////////////////
//                    char *dim_name;
//                    char *full_path_name = get_full_path_name(dim_var->name, dim_var->path);
//                    dim_name = findFixedName(fm, full_path_name);
//                    free(full_path_name);
/////////////////////////////////////////////////////////
                    char *dim_name = dim_var->name;

                    int dim_size = bp_get_type_size(dim_var->type, NULL);
                    
                    // get the value of it                              
                    FMField *dim_field = internal_find_field(dim_name, flist);

//                    free(dim_name); 

                    // TODO: cast data type
                    switch(dim_var->type) {
                        case adios_short:
                        {
                            short int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_offset = (uint64_t) temp;
                            break;
                        }
                        case adios_unsigned_short:
                        {
                            unsigned short temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_offset = (uint64_t) temp;
                            break;
                        }
                        case adios_integer:
                        {
                            int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_offset = (uint64_t) temp;
                            break;
                        }
                        case adios_unsigned_integer:
                        {
                            unsigned int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_offset = (uint64_t) temp;
                            break;
                        }
                        case adios_long:
                        {
                            long int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_offset = (uint64_t) temp;
                            break;
                        }
                        case adios_unsigned_long:
                        {
                            unsigned long int temp;
                            memcpy(&temp, &fm->buffer[dim_field->field_offset], dim_size);
                            global_offset = (uint64_t) temp;
                            break;
                        }
                        default:
                            fprintf(stderr, "cannot support data type %d as dimension.", dim_var->type);
                    }

                }
                else { //its a number
                    global_offset = d->local_offset.rank;
                }                                  
                
                // copy dimension info
                memcpy(current_pos, &local_bound, sizeof(uint64_t));
                current_pos += sizeof(uint64_t); 
                memcpy(current_pos, &global_bound, sizeof(uint64_t));
                current_pos += sizeof(uint64_t); 
                memcpy(current_pos, &global_offset, sizeof(uint64_t));
                current_pos += sizeof(uint64_t);               
            }                                                    
            
            *no_dim_pos = num_dims; // no of dimensions
            var_info_size += num_dims * sizeof(uint64_t) * 3;
            memcpy(start_pos, &var_info_size, sizeof(int)); // size of var info
        } 
    }

    return buf;
}

void
internal_adios_datatap_write(struct adios_file_struct *fd,
                             struct adios_method_struct *method)
{
    if (fd == NULL)
    {
        fprintf(stderr, "fd is null\n");
        return;
    }

    dmd *mdata = method->method_data;
    struct adios_group_struct *t = method->group;

    // SHM transport
    if(mdata->use_shm_transport == adios_flag_yes) {
        char *pos = shm_q_get_data_in_slot(mdata->shm->data_q->queue, mdata->shm->slot);
        //int temp_int = mdata->shm->current_pos_meta - pos; // we will use the maximal size
        memcpy(pos, &(mdata->shm->meta_data_size), sizeof(int));  // total meta data size   
        pos += sizeof(int);
        memcpy(pos, &(t->adios_host_language_fortran), sizeof(enum ADIOS_FLAG)); // host language
        pos += sizeof(enum ADIOS_FLAG);
        int temp_int = strlen(t->name) + 1;
        memcpy(pos, &temp_int, sizeof(int));  // group name length   
        pos += sizeof(int);
        memcpy(pos, t->name, temp_int);  // group name
        pos += temp_int;
        memcpy(pos, &(mdata->num_vars), sizeof(int)); // total num of vars written

        // mark current slot in local data queue as ready for reading and advance to next slot
        shm_q_set_current_slot_full(mdata->shm->data_q->queue);

        // update meta-data for this timestep
        if(mdata->opencount == 1) { // first timestep
            mdata->shm->file = create_new_file(fd->name,
                                               t->name,
                                               0, // TODO: start time index
                                               t->adios_host_language_fortran,
                                               rank, // TODO
                                               mdata->shm->data_q,
                                               mdata->shm->coordinator
                                              );
        } 
        else { // append new timestep to existing file
            int rc = append_to_file(mdata->shm->file, 
                                    mdata->opencount-1,  
                                    mdata->shm->coordinator
                                   );
        } 

        mdata->num_vars = 0;
        return;
    }

    // RDMA transport
    struct fm_structure *fm = mdata->fm;

    if (t == NULL || fm == NULL)
    {
        fprintf(stderr, "improperly initialized for write\n");
        return;
    }

//#if defined(__CRAYXT_COMPUTE_LINUX_TARGET)
#if HAVE_PORTALS == 1
    if (fm->snd_count > 0)
    {
        send_end(fm->s);
    }
#elif HAVE_INFINIBAND == 1
    if (fm->snd_count > 0)
    {
        EVthin_ib_endSend(fm->s);
    }
#endif

//#if defined(__CRAYXT_COMPUTE_LINUX_TARGET)
#if HAVE_PORTALS == 1

    // pack the variable info into a contiguous buffer and attach to request
    fm->var_info_buffer = create_var_info_buffer(t, fm, mdata->num_vars); 

    //start_send(fm->s, fm->buffer, fm->size, fm->ioformat, fm->alist,fm->var_info_buffer); 
    start_send(fm->s, fm->buffer, fm->size, fm->ioformat, NULL,fm->var_info_buffer); 
    free(fm->var_info_buffer);

    //recycle all temp arrays
    struct adios_var_struct *f;
    FMField *field = NULL;
    FMFieldList flist = fm->format->field_list;
    //for (f = vars; f != NULL; f = f->next) {
    while(f=next_var_from_list(&(fm->vars_to_write))) {
        char *full_path_name = get_full_path_name(f->name, f->path);
        field = internal_find_field(full_path_name, flist);
        free(full_path_name);

        int ndims = 0;
        struct adios_dimension_struct *dim = f->dimensions;
        int dim_are_numbers = 1;
        for(; dim != NULL; dim = dim->next) {
            if(dim->dimension.time_index != adios_flag_yes) {
                ndims ++;
            }
            if(dim->dimension.id) {
                dim_are_numbers = 0;
            } 
        }

        if(ndims) { // arrays
            if(dim_are_numbers) {
                void *temp_array;
                memcpy(&temp_array, &fm->buffer[field->field_offset], sizeof(void *));
                free(temp_array);   
            }  
        }        
        if(f->type == adios_string) {
            void *temp_array;
            memcpy(&temp_array, &fm->buffer[field->field_offset], sizeof(void *));
            free(temp_array);
        }
    } 

#elif HAVE_INFINIBAND == 1
//     fprintf(stderr, "size = %d\n", fm->size);
 
    // added by zf2: do not use var_info buffer
//    EVthin_ib_startSend(fm->s, fm->buffer, fm->size, fm->ioformat, fm->alist);
//    EVthin_ib_startSend(fm->s, fm->buffer, fm->size, fm->ioformat);

    EVthin_ib_startSend(fm->s, fm->buffer, fm->size, fm->ioformat, 
                        rank, mpisize, fm->snd_count, t->adios_host_language_fortran,
                        fd->name, t->name);

#endif

    mdata->num_vars = 0;
    reset_var_list(&(fm->vars_to_write));

    // added by zf2: do not use var_info buffer
//    fm->var_info_buffer_size = 2*sizeof(int);

//fprintf(stderr, "im here rank=%d %s:%d\n", rank, __FILE__,__LINE__);
//MPI_Barrier(MPI_COMM_WORLD);
//fprintf(stderr, "im here rank=%d %s:%d\n", rank, __FILE__,__LINE__);

    fm->snd_count++;
}

extern void
adios_datatap_finalize(int mype, struct adios_method_struct *method)
{
    dmd *mdata = method->method_data;
    struct adios_group_struct *t = method->group;

    // SHM transport
    if(mdata->use_shm_transport == adios_flag_yes) {
        int rc = close_file_w(mdata->shm->file, mdata->shm->coordinator);

        // TODO: cleanup
         
        return;
    }

    // RDMA transport
    struct fm_structure *fm = mdata->fm;

    if (t == NULL || fm == NULL)
    {
        fprintf(stderr, "improperly initialized for finalize %p %p\n", t, fm);
        return;
    }

#if HAVE_PORTALS == 1
    if (fm->snd_count > 0)
    {
        int appid, was_set;
        appid = globals_adios_get_application_id(&was_set);
        char buffer[128];
        sprintf(buffer, "client_timing.%d.%d", appid, rank);
        outputTimingInfo(buffer);

        send_end(fm->s);
        // TODO: we need a way to terminate writers:
        // we set some state flag and push to dt server side so it
        // knows it's the last piece and delivers "EOF" to reader app
        end_simulation(fm->s);

    }
#elif HAVE_INFINIBAND == 1

    if (fm->snd_count > 0)
    {

        char buffer[128];
        sprintf(buffer, "client-%d", rank);

        EVthin_ib_outputTimingInfo(buffer);

        //sleep(5);
        EVthin_ib_endSend(fm->s);

        MPI_Barrier(adios_mpi_comm_world);
        // send out EoF message
        EVthin_ib_endSimulation(fm->s);
    }

#endif
}


extern void
adios_datatap_end_iteration(struct adios_method_struct *method)
{
    struct fm_structure *fm;
    dmd *mdata = (dmd *) method->method_data;
    fm = mdata->fm;

    if (fm == NULL)
        return;


#if HAVE_PORTALS == 1
    startIter(fm->s);
#elif HAVE_INFINIBAND == 1
    EVthin_ib_startIter(fm->s);
#endif

    mdata->cycle_id = 0;

}

extern void
adios_datatap_start_calculation(struct adios_method_struct *method)
{
    struct fm_structure *fm;
    dmd *mdata = (dmd *) method->method_data;
    fm = mdata->fm;

    if (fm == NULL)
        return;


#if HAVE_PORTALS == 1
    startCompute(fm->s, mdata->cycle_id);
#elif HAVE_INFINIBAND == 1
    EVthin_ib_startCompute(fm->s, mdata->cycle_id);
#endif


}

extern void
adios_datatap_stop_calculation(struct adios_method_struct *method)
{
    struct fm_structure *fm;
    dmd *mdata = (dmd *) method->method_data;
    fm = mdata->fm;

    if (fm == NULL)
        return;


#if HAVE_PORTALS == 1
    endCompute(fm->s, mdata->cycle_id);
#elif HAVE_INFINIBAND == 1
    EVthin_ib_endCompute(fm->s, mdata->cycle_id);
#endif

    mdata->cycle_id++;

}

extern void
adios_datatap_get_write_buffer(struct adios_file_struct *fd,
                   struct adios_var_struct *f,
                   uint64_t *size,
                   void **buffer,
                   struct adios_method_struct *method)
{
    fprintf(stderr, "adios_datatap_write_get_buffer: datatap disabled, "
            "no portals support\n");
}


void
adios_datatap_read(struct adios_file_struct *fd,
           struct adios_var_struct *f,
           void *buffer, uint64_t buffer_size,
           struct adios_method_struct *method)
{

}

#else

void
adios_datatap_read(struct adios_file_struct *fd,
                   struct adios_var_struct *f,
                   void *buffer, struct adios_method_struct *method)
{

}

extern void
adios_datatap_get_write_buffer(struct adios_file_struct *fd,
                               struct adios_var_struct *f,
                               unsigned long long *size,
                               void **buffer,
                               struct adios_method_struct *method)
{
}

extern void
adios_datatap_stop_calculation(struct adios_method_struct *method)
{
}

extern void
adios_datatap_start_calculation(struct adios_method_struct *method)
{
}

extern void
adios_datatap_end_iteration(struct adios_method_struct *method)
{
}

extern void
adios_datatap_finalize(int mype, struct adios_method_struct *method)
{
}

extern void
adios_datatap_close(struct adios_file_struct *fd,
                    struct adios_method_struct *method)
{
}

extern void
adios_datatap_write(struct adios_file_struct *fd,
                    struct adios_var_struct *f,
                    void *data, struct adios_method_struct *method)
{
}

extern void
adios_datatap_open(struct adios_file_struct *fd,
                   struct adios_method_struct *method)
{
}

extern void
adios_datatap_init(const char *params, struct adios_method_struct *method)
{
}

enum ADIOS_FLAG adios_datatap_should_buffer (struct adios_file_struct * fd
                                            ,struct adios_method_struct * method)
{
    return 0;
}
#endif

