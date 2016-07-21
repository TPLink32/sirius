#include <stdint.h>
#include <mpi.h>
#include <string.h>
#include "sirius.h"
#include "jsonparser.h"

//===============================================================================================

// read the JSON file to initialize the internal output information
int sirius_init (struct SIRIUS_WRITE_OPTIONS * write_handle, MPI_Comm comm, const char * config)
{
	int i = 0, n = 0;

	uint32_t global_size;
	uint32_t global_rank;
	
	char** dim_index_str_arr = NULL;
	char** var_index_str_arr = NULL;
	
	int nc_err = MPI_Comm_size(comm, &global_size);
	nc_err = MPI_Comm_rank(comm, &global_rank);
	
	json_object** json_obj_arr_ptr = NULL;
	
	
	if(global_rank == ROOT_PROCESS)
	{
		json_obj_arr_ptr = parseFileWithName(config);		
		if(json_obj_arr_ptr == NULL)	exit(0);
		
		json_object* temp = json_obj_arr_ptr[0];
		
		while(temp != NULL)
		{
			char* current_obj_id = removeQuotesOnString(getObjID(temp));
			
			if(strcmp(current_obj_id, "globalParams") == 0)
			{
				write_handle->num_dims = atoi(getValueAsStringFromObjAndKey(temp, "dimensions"));
				write_handle->global_dimensions = malloc(sizeof(uint64_t) * write_handle->num_dims);
				
				write_handle->num_vars = atoi(getValueAsStringFromObjAndKey(temp, "variables"));
				write_handle->var_dimensions = malloc(sizeof(uint64_t) * write_handle->num_vars);
				
				write_handle->max_priority = atoi(getValueAsStringFromObjAndKey(temp, "maxPriority"));
				
				dim_index_str_arr = alloc_and_get_index_str_arr('d', write_handle->num_dims);
				var_index_str_arr = alloc_and_get_index_str_arr('v', write_handle->num_vars);
				
				MPI_Bcast(&write_handle->num_dims, 1, MPI_UNSIGNED_LONG, ROOT_PROCESS, comm);
				MPI_Bcast(&write_handle->num_vars, 1, MPI_UNSIGNED_LONG, ROOT_PROCESS, comm);
			}
			else
			{
				for(n = 0; n < write_handle->num_dims; n++)
				{
					if(strcmp(current_obj_id, dim_index_str_arr[n]) == 0)
					{
						//printf("got dimension: %s\n", dim_index_str_arr[n]);
						//dimStrList[n] = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "name"));
						write_handle->global_dimensions[n] = atoi(getValueAsStringFromObjAndKey(temp, "size"));
						
						MPI_Bcast(&write_handle->global_dimensions[n], 1, MPI_UNSIGNED_LONG_LONG, ROOT_PROCESS, comm);
						//printf("%s: size: %d\n",dimStrList[n], (int)dim_Sizes[n]);
					}
				}
				for(n = 0; n < write_handle->num_vars; n++)
				{
					if(strcmp(current_obj_id, var_index_str_arr[n]) == 0)
					{
						//printf("got dimension: %s\n", dim_index_str_arr[n]);
						//dimStrList[n] = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "name"));
						write_handle->var_dimensions[n] = atoi(getValueAsStringFromObjAndKey(temp, "dims"));
						
						MPI_Bcast(&write_handle->var_dimensions[n], 1, MPI_UNSIGNED_LONG_LONG, ROOT_PROCESS, comm);
						//printf("%s: size: %d\n",dimStrList[n], (int)dim_Sizes[n]);
					}
				}
			}
			i++;
			temp = json_obj_arr_ptr[i];
		}
		free(dim_index_str_arr);
		free(var_index_str_arr);
	}
	
	//the Bcasts embedded in this "if" are because they are called by the root proc above already
	if(global_rank != ROOT_PROCESS)
	{
		MPI_Bcast(&write_handle->num_dims, 1, MPI_UNSIGNED_LONG, ROOT_PROCESS, comm);
		MPI_Bcast(&write_handle->num_vars, 1, MPI_UNSIGNED_LONG, ROOT_PROCESS, comm);
		
		write_handle->global_dimensions = malloc(sizeof(uint64_t) * write_handle->num_dims);
		write_handle->var_dimensions = malloc(sizeof(uint64_t) * write_handle->num_vars);
	
		for(n = 0; n < write_handle->num_dims; n++)
		{	
			MPI_Bcast(&write_handle->global_dimensions[n], 1, MPI_UNSIGNED_LONG_LONG, ROOT_PROCESS, comm);
			//printf("Dims: %d\n", (int)write_handle->global_dimensions[n]);
		}
		for(n = 0; n < write_handle->num_vars; n++)
		{
			MPI_Bcast(&write_handle->var_dimensions[n], 1, MPI_UNSIGNED_LONG_LONG, ROOT_PROCESS, comm);
			//printf("Var dims: %d\n", (int)write_handle->var_dimensions[n]);
		}
	}
	
	//these are called by all procs, here
	MPI_Bcast(&write_handle->max_priority, 1, MPI_INT, ROOT_PROCESS, comm);
	
    return 1;
}

// clean up anything configured on sirius_init and make sure anything reserved is freed.
int sirius_finalize (struct SIRIUS_WRITE_OPTIONS * write_handle, int rank)
{
	if(rank == ROOT_PROCESS)
		free (write_handle->global_dimensions);
    return 0;
}

//===============================================================================================

// open an output stream for writing. Kept separate from reading for simplicity of implementation
// handle is an output parameter
static int DumbGlobalCounter = 0;
int sirius_open_write (struct SIRIUS_HANDLE * handle, const char * name, MPI_Comm * comm)
{
	int nc_err; 
	//nc_err = MPI_Comm_size(*comm, &handle->comm_size); 
	handle->runtime_config_filename = name;
	handle->comm = comm;
	handle->handle = DumbGlobalCounter++; 
	handle->op_type = READ_OP;
	//intentional post increment	

    return 0;
}

// request a valid for a limited time only list of options of where to write the data
// the assumption is that the programmer can decide how much time is acceptable and consider
// previous outputs to determine what and how much to write during this output
int sirius_get_write_time_estimates (struct SIRIUS_HANDLE * handle, size_t total_data_size, int * options, struct SIRIUS_TIME * times)
{
    return 0;
}

// write a var to a stream using a particular reservation (or NULL if no particular choice).
// var_handle is an output parameter for use with the priority regions or other such encodings.
int sirius_write (struct SIRIUS_HANDLE * handle, struct SIRIUS_RESERVATION_HANDLE * res, const char * var_name, void * data, struct SIRIUS_VAR_HANDLE * var_handle)
{
	var_handle->var_name = var_name;
	struct SIRIUS_VARINFO* handle_varinfo = var_handle->var_info = malloc(sizeof(struct SIRIUS_VARINFO));
	
	//This string checking is in lieu of actually reading the JSON file, for now
	if(strcmp(var_name, "temperature") == 0)
	{
		handle_varinfo->type = sirius_double;
		var_handle->handle = handle_varinfo->varid = 0;
		handle_varinfo->ndim = 3;
		handle_varinfo->dims = malloc(sizeof(uint64_t) * handle_varinfo->ndim);
		handle_varinfo->dims[0] = LENGTH;
		handle_varinfo->dims[1] = WIDTH;
		handle_varinfo->dims[2] = HEIGHT;

		handle_varinfo->nsteps = 1;
		printf("wrote temp\n");
	}
	else if(strcmp(var_name, "pressure") == 0)
	{
		handle_varinfo->type = sirius_double;
		var_handle->handle = handle_varinfo->varid = 1;
		handle_varinfo->ndim = 3;
		handle_varinfo->dims = malloc(sizeof(uint64_t) * handle_varinfo->ndim);
		handle_varinfo->dims[0] = LENGTH;
		handle_varinfo->dims[1] = WIDTH;
		handle_varinfo->dims[2] = HEIGHT;

		handle_varinfo->nsteps = 1;
		printf("wrote pressure\n");
	}
	
    return 1;
}

// write a list of one or more priority regions for a particular process.  The coordinates are
// assuming a 3-d cartesian space and all values are in the global rather than local space.
int sirius_write_priority_regions (struct SIRIUS_HANDLE * handle, uint32_t * count, uint64_t * start_coords, uint64_t * end_coords, struct SIRIUS_VAR_HANDLE * var_handle)
{	
	int ndims = get_var_dims_from_var_handle(var_handle);
	//printf("dims: %d\n", ndims);
	int i, n;

	struct SIRIUS_PRIORITY_REGION* temp_pri_region = NULL;
	
	for(i = 0; i < *count; i++)
	{
		if(i == 0) //set the head
		{
			var_handle->priority_head = malloc(sizeof(struct SIRIUS_PRIORITY_REGION));
			temp_pri_region = var_handle->priority_head;
		}
		else //otherwise, allocate another element and adjust current ptr
		{
			temp_pri_region->next = malloc(sizeof(struct SIRIUS_PRIORITY_REGION));
			temp_pri_region = temp_pri_region->next;
		}
	
		temp_pri_region->dims = ndims;
		temp_pri_region->start_coords = malloc(sizeof(uint64_t) * ndims);
		temp_pri_region->end_coords = malloc(sizeof(uint64_t) * ndims);

		//printf("Pri region:\n");
		for(n = 0; n < ndims; n++)
		{
			//printf("   %d - %d\n", (int)start_coords[(i * ndims) + n], (int)end_coords[(i * ndims) + n]);
			temp_pri_region->start_coords[n] = start_coords[(i * ndims) + n];
			temp_pri_region->end_coords[n] = end_coords[(i * ndims) + n];
		}	
		temp_pri_region->next = NULL;
	}
	//metadata_write_priority(uint32_t count, uint64_t start_coords, uint64_t end_coords, int num_dims, struct SIRIUS_VAR_HANDLE * var_handle);
    return 1;
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
	//TODO
	//use the json parsed info from before 
	 
	if(strcmp(var_name, "temperature") == 0)
	{
		info->varid = 0;
		info->type = sirius_double;
		info->ndim = 3;
		info->dims = malloc(sizeof(uint64_t) * info->ndim);
		info->dims[0] = LENGTH;
		info->dims[1] = WIDTH;
		info->dims[2] = HEIGHT;

		info->nsteps = 1;
		//printf("wrote temp\n");
	}
	else if(strcmp(var_name, "pressure") == 0)
	{
		info->varid = 1;
		info->type = sirius_double;
		info->ndim = 3;
		info->dims = malloc(sizeof(uint64_t) * info->ndim);
		info->dims[0] = LENGTH;
		info->dims[1] = WIDTH;
		info->dims[2] = HEIGHT;

		info->nsteps = 1;
		//printf("wrote pressure\n");
	} 
	 
    return 1;
}

//TODO requests the metadata server for a variable handle associated with a particular name
int sirius_get_var_handle(const char* var_name, struct SIRIUS_VAR_HANDLE * var_handle)
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
	handle->op_type = WRITE_OP;
	handle->comm = readers_comm;
	handle->handle = DumbGlobalCounter++; 
	
	
    return 1;
}

// read the requested_parameters portion of the var. Put no more than buffer_size bytes into the buffer
int sirius_read (struct SIRIUS_HANDLE * handle, struct SIRIUS_RESERVATION_HANDLE * res, struct SIRIUS_VARINFO * requested_parameters, size_t buffer_size, void * buffer)
{
	if(requested_parameters->varid == 0)
	{
		debug_fill_buffer("temperature", buffer_size, buffer);
	}
	else if(requested_parameters->varid == 1)
	{
		debug_fill_buffer("pressure", buffer_size, buffer);
	}

    return 1;
}

static char** alloc_and_get_index_str_arr(char base, int num)
{
	int n;
	char** ret = malloc(sizeof(char*) * num);
	
	for(n = 0; n < num; n++)
	{
		char app[1];
		app[0] = (n + 1) + '0';
		
		ret[n] = malloc(sizeof(char) * 6);
		if(base == 'd')
			strcpy(ret[n], "dim");
		else if(base == 'v')
			strcpy(ret[n], "var");
		else 
			strcpy(ret[n], "nul");
			
			
		strcat(ret[n], app);
		printf("%s\n", ret[n]);
	}

	return ret;
}

static int get_var_dims_from_var_handle(struct SIRIUS_VAR_HANDLE* var_handle)
{
	return var_handle->var_info->ndim;
}

static void debug_fill_buffer(const char* name, size_t size, double* buffer)
{
	int i;

	if(strcmp(name, "temperature") == 0)
	{
		for(i = 0; i < size; i++)
		{
			buffer[i] = i;
		}
	}
	else if (strcmp(name, "pressure") == 0)
	{
		for(i = 0; i < size; i++)
		{
			buffer[i] = size - i;
		}
	}
}

void debug_print_buffer(void* buffer, const char type, size_t size)
{
	int i;
	switch(type)
	{
		case 'd':
			printf("buffer:\n");
			for(i = 0; i < size; i++)
			{
				printf("%0.0f, ", ((double*)buffer)[i]);
			}
			printf("\n");
			return;
	}
}
void print_linked_priority_regions(const char* var_name, struct SIRIUS_PRIORITY_REGION * head)
{
	if(head == NULL)	return;
	
	int q;
	printf("%s region:\n", var_name);
	for(q = 0; q < head->dims; q++)
	{
		printf("%d to %d\n", (int)head->start_coords[q], (int)head->end_coords[q]);
	}
	print_linked_priority_regions(var_name, head->next);
}

