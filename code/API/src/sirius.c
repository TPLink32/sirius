#include <stdint.h>
#include <mpi.h>
#include "sirius.h"

//===============================================================================================

// read the JSON file to initialize the internal output information
int sirius_init (MPI_Comm comm, const char * config)
{
    return 0;
}

// clean up anything configured on sirius_init and make sure anything reserved is freed.
int sirius_finalize (int rank)
{
    return 0;
}

//===============================================================================================

// open an output stream for writing. Kept separate from reading for simplicity of implementation
// handle is an output parameter
int sirius_open_write (struct SIRIUS_HANDLE * handle, const char * name, MPI_Comm * comm)
{
    return 0;
}

// request a valid for a limited time only list of options of where to write the data
// the assumption is that the programmer can decide how much time is acceptable and consider
// previous outputs to determine what and how much to write during this output
int sirius_get_write_time_estimates (struct SIRIUS_HANDLE * handle, size_t total_data_size, int * options, struct SIRIUS_TIME * times)
{
    return 0;
}

// write a var to a stream using a partiular reservation (or NULL if no particular choice).
// var_handle is an output parameter for use with the priority regions or other such encodings.
int sirius_write (struct SIRIUS_HANDLE * handle, struct SIRIUS_RESERVATION_HANDLE * res, const char * var_name, void * data, struct SIRIUS_VAR_HANDLE * var_handle)
{
    return 0;
}

// write a list of one or more priority regions for a particular process.  The coordinates are
// assuming a 3-d cartaesan space and all values are in the global rather than local space.
int sirius_write_priority_regions (struct SIRIUS_HANDLE * handle, uint32_t * count, uint64_t * coords)
{
    return 0;
}

// close a stream to force cleaning up and committing anything that might remain from an output
int sirius_close (struct SIRIUS_HANDLE * handle)
{
    return 0;
}

//===============================================================================================

// get information about a variable so that the user can allocate buffers and make decisions about
// what portion of the var they care about.
int sirius_get_var_info (const char * stream_name, const char * var_name, struct SIRIUS_VARINFO * info)
{
    return 0;
}

// get options for reading a particular var from a particular output stream
// assuming that the options_count on input specifies how many options (max) are requested and
// allocated in the options pointer.
// This is changed on output to the number of options available and the first this many options are
// populated
int sirius_get_read_options (const char * stream_name, const char * var_name, uint8_t * options_count, struct SIRIUS_READ_OPTIONS * options)
{
    return 0;
}

// open a stream for reading. The name is some encoded way to identify the actual thing we want
// handle is an output parameter
int sirius_open_read (MPI_Comm * readers_comm, const char * name, struct SIRIUS_RESERVATION_HANDLE * res, struct SIRIUS_HANDLE * handle)
{
    return 0;
}

// read the requested_parameters portion of the var. Put no more than buffer_size bytes into the buffer
int sirius_read (struct SIRIUS_HANDLE * handle, struct SIRIUS_RESERVATION_HANDLE * res, struct SIRIUS_VARINFO * requested_parameteres, size_t buffer_size, void * buffer)
{
    return 0;
}
