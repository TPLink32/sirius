#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct SIRIUS_HANDLE
{
    uint64_t handle;
};

struct SIRIUS_VAR_HANDLE
{
    uint64_t handle;
};

struct SIRIUS_RESERVATION_HANDLE
{
    uint64_t handle;
};

struct SIRIUS_TIME
{
    struct SIRIUS_RESERVATION_HANDLE id;
    time_t time;
};

struct SIRIUS_READ_OPTIONS
{
    struct SIRIUS_RESERVATION_HANDLE res; // ID to specify using this option
    struct SIRIUS_TIME time_max; // maximum possible read time
    struct SIRIUS_TIME time_expected; // expected read time
    uint8_t time_confidence; // a 0-100 confidence for ability to achieve the time_expected
    uint64_t size;  // size of this output
    int encoding; // how this option is encoded
    void * encoding_parameters; // a per-encoding set of parameters that make sense in context
};

enum SIRIUS_DATATYPES {sirius_unknown = -1             /* (size) */

                     ,sirius_byte = 0                 /* (1) */
                     ,sirius_short = 1                /* (2) */
                     ,sirius_integer = 2              /* (4) */
                     ,sirius_long = 4                 /* (8) */

                     ,sirius_unsigned_byte = 50       /* (1) */
                     ,sirius_unsigned_short = 51      /* (2) */
                     ,sirius_unsigned_integer = 52    /* (4) */
                     ,sirius_unsigned_long = 54       /* (8) */

                     ,sirius_real = 5                 /* (4) */
                     ,sirius_double = 6               /* (8) */
                     ,sirius_long_double = 7          /* (16) */

                     ,sirius_string = 9               /* (?) */
                     ,sirius_complex = 10             /* (8) */
                     ,sirius_double_complex = 11      /* (16) */

                     /* Only for attributes: char** array of strings.
                        Number of elements must be known externally */
                     ,sirius_string_array = 12        /* (sizeof(char*)) usually 4 */
                     };

struct SIRIUS_VARINFO
{
        int        varid;           /* variable index (0..ADIOS_FILE.nvars-1)                         */
        enum SIRIUS_DATATYPES type;  /* type of variable                                               */
        int        ndim;            /* number of dimensions, 0 for scalars                            */
        uint64_t * dims;            /* size of each dimension.
                                       If variable has no global view 'dims' report the size of the
                                       local array written by process rank 0.
                                    */
        int        nsteps;          /* Number of steps of the variable in file.
                                       There is always at least one step.                             */
                                    /* In streams it always equals 1.                                 */
        void     * value;           /* value of a scalar variable, NULL for array.                    */
        int        global;          /* 1: global view (was defined by writer),
                                       0: pieces written by writers without defining a global array   */
        int      * nblocks;         /* Number of blocks that comprise this variable in a step
                                       It is an array of 'nsteps' integers                            */
        int        sum_nblocks;     /* Number of all blocks of all steps, the sum of the nblocks array*/
        int        nattrs;          /* Number of attributes with the name <variable_fullpath>/<name>  */
        int      * attr_ids;        /* Attribute ids in an array, use fp->attr_namelist[<id>] for names */
//        ADIOS_VARSTAT  *statistics; /* Statistics, retrieved in separate call: adios_inq_var_stat()   */
//        ADIOS_VARBLOCK *blockinfo;  /* Spatial arrangement of written blocks,
//                                       retrieved in separate call: adios_inq_var_blockinfo()
//                                       It is an array of 'sum_nblocks' elements                       */
//        ADIOS_VARMESH *meshinfo;    /* structure in this file,
//                                       retrieved in separate call: adios_inq_var_meshinfo()          */
};

//===============================================================================================

// read the JSON file to initialize the internal output information
int sirius_init (MPI_Comm comm, const char * config);

// clean up anything configured on sirius_init and make sure anything reserved is freed.
int sirius_finalize (int rank);

//===============================================================================================

// open an output stream for writing. Kept separate from reading for simplicity of implementation
// handle is an output parameter
int sirius_open_write (struct SIRIUS_HANDLE * handle, const char * name, MPI_Comm * comm);

// request a valid for a limited time only list of options of where to write the data
// the assumption is that the programmer can decide how much time is acceptable and consider
// previous outputs to determine what and how much to write during this output
int sirius_get_write_time_estimates (struct SIRIUS_HANDLE * handle, size_t total_data_size, int * options, struct SIRIUS_TIME * times);

// write a var to a stream using a partiular reservation (or NULL if no particular choice).
// var_handle is an output parameter for use with the priority regions or other such encodings.
int sirius_write (struct SIRIUS_HANDLE * handle, struct SIRIUS_RESERVATION_HANDLE * res, const char * var_name, void * data, struct SIRIUS_VAR_HANDLE * var_handle);

// write a list of one or more priority regions for a particular process.  The coordinates are
// assuming a 3-d cartaesan space and all values are in the global rather than local space.
int sirius_write_priority_regions (struct SIRIUS_HANDLE * handle, uint32_t * count, uint64_t * coords);

// close a stream to force cleaning up and committing anything that might remain from an output
int sirius_close (struct SIRIUS_HANDLE * handle);

//===============================================================================================

// get information about a variable so that the user can allocate buffers and make decisions about
// what portion of the var they care about.
int sirius_get_var_info (const char * stream_name, const char * var_name, struct SIRIUS_VARINFO * info);

// get options for reading a particular var from a particular output stream
// assuming that the options_count on input specifies how many options (max) are requested and
// allocated in the options pointer.
// This is changed on output to the number of options available and the first this many options are
// populated
int sirius_get_read_options (const char * stream_name, const char * var_name, uint8_t * options_count, struct SIRIUS_READ_OPTIONS * options);

// open a stream for reading. The name is some encoded way to identify the actual thing we want
// handle is an output parameter
int sirius_open_read (MPI_Comm * readers_comm, const char * name, struct SIRIUS_RESERVATION_HANDLE * res, struct SIRIUS_HANDLE * handle);

// read the requested_parameters portion of the var. Put no more than buffer_size bytes into the buffer
int sirius_read (struct SIRIUS_HANDLE * handle, struct SIRIUS_RESERVATION_HANDLE * res, struct SIRIUS_VARINFO * requested_parameteres, size_t buffer_size, void * buffer);
