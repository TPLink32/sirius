
#include "jsonparser.h"
#include "groupTest.h"

//compile w/ "mpicc groupTest.c -lm -lpnetcdf -o "exe_name" "
//run w/ "mpirun -np x ./group" where x is # of processes

struct priorityRegion
{
	char tag;
	int start[3];
	int end[3];
	int priority_level;
	pri_region_link* next;
};

static FILE* LOG_FILE = NULL;

void main(int argc, char** argv)
{
	void* data = getTestDataBuffer(5, 10, 15, 20);
	int i;
	for(i = 0; i < 5*10*15*20; i += 5)
	{
		//printf("(%0.0f, %0.0f, %0.0f): %0.0f, %0.0f\n", 
		//((double*)data)[i], ((double*)data)[i+1], ((double*)data)[i+2], ((double*)data)[i+3], ((double*)data)[i+4]);
	}
	//printf("\n");
	int nc_err = MPI_Init(&argc, &argv);
	
	mainFunc(data, (5*10*15*20), argc, argv);
	free(data);
	
	nc_err = MPI_Finalize();
	//if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (END)\n", global_rank, ncmpi_strerror (nc_err));	
}

void mainFunc(void* inputData, long inputDataSize, int argc, char** argv)
{	
/*****************************************************************************/
	char** dimStrList = malloc(sizeof(char*) * 3);
	dimStrList[0] = malloc(sizeof(char) * DIM_CHAR_LENGTH);
	dimStrList[1] = malloc(sizeof(char) * DIM_CHAR_LENGTH);
	dimStrList[2] = malloc(sizeof(char) * DIM_CHAR_LENGTH);
	
	char** varStrList = malloc(sizeof(char*) * 2);
	varStrList[0] = malloc(sizeof(char) * DIM_CHAR_LENGTH);
	varStrList[1] = malloc(sizeof(char) * DIM_CHAR_LENGTH);
	
	char** dimIndexStrList = malloc(sizeof(char*) * 3);
	dimIndexStrList[0] = "dim1";
	dimIndexStrList[1] = "dim2";
	dimIndexStrList[2] = "dim3";
	
	char** varIndexStrList = malloc(sizeof(char*) * 2);
	varIndexStrList[0] = "var1";
	varIndexStrList[1] = "var2";

	
	int* dim_Ids = NULL;
	int* var_Ids = NULL;
	
	int* dim_Mins = NULL; //offsets for local
	
	MPI_Offset* dim_Sizes = NULL;
	MPI_Offset* var_Dim_Sizes = NULL;

	int i = 0, n = 0, w = 0;
	
	int nc_err;
	int ncid;
	
	int grav_x_c_varid;

	MPI_Offset start[10];
	MPI_Offset readsize[10];
    int n_procs[10];
    int dims[10];
	 
	MPI_Offset nx;
	MPI_Offset ny;
	MPI_Offset nz;
	
	char dim_name[100];
	
	int global_rank, global_size;
	int local_rank, local_size;
	
	char* nc5_file_name = NULL;
	int num_dims;
	int num_vars;
	int max_pri;
	
/*****************************************************************************/
	//MPI_Init is called by main, before calling this mainFunc
	nc_err = MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);
	nc_err = MPI_Comm_size(MPI_COMM_WORLD, &global_size);
	
	MPI_Comm no_priority_comm;
	MPI_Comm priority_comm;
	
	json_object** json_obj_arr_ptr = NULL;
	json_object** global_params_obj = NULL;

	srand(global_rank * time(NULL));
	int rand_pri_region_count = getRandNumPriorityRegions(MAX_PRI_REGIONS, PERCENT_0_PRI_BIAS);
	
	if(global_rank == ROOT_PROCESS)
	{	
		json_obj_arr_ptr = parseFileWithName(JSON_FILE_NAME);
		
		json_object* temp = json_obj_arr_ptr[0];
		
		while(temp != NULL)
		{
			//printObj(temp);
			char* current_obj_id = removeQuotesOnString(getObjID(temp));
			//printf("%s\n", getObjID(temp));
			
			if(strcmp(current_obj_id, "globalParams") == 0)
			{
				global_params_obj = &temp;
				nc5_file_name = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "fileWriteName"));
				num_dims = atoi(getValueAsStringFromObjAndKey(temp, "dimensions"));
				num_vars = atoi(getValueAsStringFromObjAndKey(temp, "variables"));
				max_pri = atoi(getValueAsStringFromObjAndKey(temp, "maxPriority"));
				
				dim_Ids = malloc(sizeof(int) * num_dims);
				var_Ids = malloc(sizeof(int) * num_vars);
				
				dim_Mins = malloc(sizeof(int) * num_dims);
				
				dim_Sizes = malloc(sizeof(MPI_Offset) * num_dims);
				var_Dim_Sizes = malloc(sizeof(MPI_Offset) * num_vars);
				//printf("got globals!\n");
			}
			else
			{
				for(n = 0; n < num_dims; n++)
				{
					if(strcmp(current_obj_id, dimIndexStrList[n]) == 0)
					{
						//printf("got dimension: %d: ", n+1);
						dimStrList[n] = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "name"));
						dim_Sizes[n] = atoi(getValueAsStringFromObjAndKey(temp, "size"));
						//printf("%s: size: %d\n",dimStrList[n], (int)dim_Sizes[n]);
					}
				}
				for(n = 0; n < num_vars; n++)
				{
					if(strcmp(current_obj_id, varIndexStrList[n]) == 0)
					{
						varStrList[n] = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "name"));
						var_Dim_Sizes[n] = atoi(getValueAsStringFromObjAndKey(temp, "dims"));
						//printf("%s: size: %d\n",dimStrList[n], (int)var_Dim_Sizes[n]);
					}
				}
			}
			i++;
			temp = json_obj_arr_ptr[i];
		}
	}
	else
	{
		nc5_file_name = malloc(sizeof(char) * FILENAME_CHAR_LENGTH);	
	}
	
	MPI_Bcast(nc5_file_name, FILENAME_CHAR_LENGTH, MPI_BYTE, ROOT_PROCESS, MPI_COMM_WORLD);
	MPI_Bcast(&num_dims, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);
	MPI_Bcast(&num_vars, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);
	MPI_Bcast(&max_pri, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);
	
	if(global_rank != ROOT_PROCESS)
	{
		dim_Ids = malloc(sizeof(int) * num_dims);
		var_Ids = malloc(sizeof(int) * num_vars);
		
		dim_Mins = malloc(sizeof(int) * num_dims);
			
		dim_Sizes = malloc(sizeof(MPI_Offset) * num_dims);
		var_Dim_Sizes = malloc(sizeof(MPI_Offset) * num_vars);
	}

	nc_err = ncmpi_create(MPI_COMM_WORLD, nc5_file_name, NC_CLOBBER|NC_64BIT_OFFSET|NC_WRITE, MPI_INFO_NULL, &ncid);
	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (1)\n", global_rank, ncmpi_strerror (nc_err)); 
	
	for(i = 0; i < num_dims; i++)
	{
		MPI_Bcast(&dim_Sizes[i], 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);
		MPI_Bcast(dimStrList[i], DIM_CHAR_LENGTH, MPI_CHAR, ROOT_PROCESS, MPI_COMM_WORLD);
		
		nc_err = ncmpi_def_dim(ncid, dimStrList[i], (int)dim_Sizes[i], &dim_Ids[i]);
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (%d)\n", global_rank, ncmpi_strerror (nc_err), i);
	}
	//shares info about the dimensions, and then defines them

	for(i = 0; i < num_vars; i++)
	{
		MPI_Bcast(varStrList[i], DIM_CHAR_LENGTH, MPI_CHAR, ROOT_PROCESS, MPI_COMM_WORLD);
		MPI_Bcast(&var_Dim_Sizes[i], 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);
	}
	//shares info about the variables
	
	for(i = 0; i < num_vars; i++)
	{
		nc_err = ncmpi_def_var(ncid, varStrList[i], NC_DOUBLE, var_Dim_Sizes[i], dim_Ids, &var_Ids[i]);
     	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (%d)\n", global_rank, ncmpi_strerror (nc_err), i);
    }
    //defines the variables formally
    
	nc_err = ncmpi_enddef(ncid);
	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (%d)\n", global_rank, ncmpi_strerror (nc_err), i);
	
	//printf("%dNS: %d, %d, %d, %d\n", global_rank, (int)dim_Sizes[0], (int)dim_Sizes[1], (int)dim_Sizes[2], (int)dim_Sizes[3]);
	
	long volume = (int)dim_Sizes[0] * (int)dim_Sizes[1] * (int)dim_Sizes[2];
	
	//printf("vol: %li\n", volume);
	
	int* myDims;
	if(isEquallyDivisibleVolume(volume, global_size))
	{
		myDims = allocAndReturnBestDims((int)dim_Sizes[0],(int)dim_Sizes[1],(int)dim_Sizes[2], global_size);
		
		if(myDims == NULL)
		{
			printf("couldn't find ideal coordinates");
			return;
		}
		else
		{
			//printf("dims: %d, %d, %d\n", myDims[0], myDims[1], myDims[2]);
			//int* test = allocAndReturnBestDims(10, 10, 10, 10);
			//printf("dims: %d, %d, %d\n", test[0], test[1], test[2]);
			//free(test);
		}
	}
	else
	{
		printf("can't evenly divide the volume, exiting");
		return;
	}

		
	n_procs[0] = (int)dim_Sizes[0] / myDims[0];
	n_procs[1] = (int)dim_Sizes[1] / myDims[1];
	n_procs[2] = (int)dim_Sizes[2] / myDims[2];
		
	dim_Mins[0] = (global_rank % n_procs[0]) * myDims[0];
	dim_Mins[1] = ((global_rank / n_procs[0]) % n_procs[1]) * myDims[1];
	dim_Mins[2] = global_rank / (n_procs[0] * n_procs[1]) * myDims[2];
		
	for(i = 0; i < num_dims; i++)
	{
		if(dim_Mins[i] + myDims[i] > (int)dim_Sizes[i])
		{
			readsize[i] = (int)dim_Sizes[i] - dim_Mins[i];
			printf("%d in %d plane shortened to %d\n", (dim_Mins[i] + myDims[i]), i, (int)readsize[i]);
		}
		else
		{
			readsize[i] = myDims[i];
		}
	}
	//printf("(%d, %d, %d)", myDims[0], myDims[1], myDims[2]);

	//isPriorityInMyRegion(priority_2D_coord_arr, rangeArr);

	if(rand_pri_region_count > 0)
	{
		pri_region_link* head_priority_link_ptr = NULL;
		pri_region_link* cur_priority_link_ptr = NULL;
		
		int* buffer = malloc(sizeof(int) * 2);
		MPI_Comm_split(MPI_COMM_WORLD, PRIORITY_COLOR, global_rank, &priority_comm);
		nc_err = MPI_Comm_rank(priority_comm, &local_rank);
		nc_err = MPI_Comm_size(priority_comm, &local_size);
		
		char curTag = accumulateTagThroughGroup('A', rand_pri_region_count, priority_comm, local_rank, local_size);

		for(n = 0; n < rand_pri_region_count; n++)
		{
			if(n == 0)
			{
				head_priority_link_ptr = malloc(sizeof(pri_region_link));
				cur_priority_link_ptr = head_priority_link_ptr;
			}
			else
			{
				cur_priority_link_ptr->next = malloc(sizeof(pri_region_link));
				cur_priority_link_ptr = cur_priority_link_ptr->next;
			}
			//adjusts the current pointer to a fresh object
			
			cur_priority_link_ptr->tag = curTag;
			curTag++;
		
			for(i = 0; i < 3; i++)
			{
				fillPriCoordPairBuffer(dim_Mins[i], dim_Mins[i] + myDims[i], buffer);
				cur_priority_link_ptr->start[i] = buffer[0];
				cur_priority_link_ptr->end[i] = buffer[1];
			}
			cur_priority_link_ptr->priority_level = rand() % max_pri;
			cur_priority_link_ptr->next = NULL;
		}
		
		//printAllPriStats(global_rank, head_priority_link_ptr);
		free(buffer);
	}	
	else
	{
		MPI_Comm_split(MPI_COMM_WORLD, NO_PRIORITY_COLOR, global_rank, &no_priority_comm);
		nc_err = MPI_Comm_rank(no_priority_comm, &local_rank);
		nc_err = MPI_Comm_size(no_priority_comm, &local_size);
	}

	//start[3] = 0; //time dimension
	start[0] = dim_Mins[0];
	start[1] = dim_Mins[1];
	start[2] = dim_Mins[2];

/*
	printf("%d has start: (%d,%d,%d) and size: (%d,%d,%d)\n", 
			global_rank, (int)start[0], (int)start[1], (int)start[2], 
			(int)readsize[0], (int)readsize[1], (int)readsize[2]);
			*/
			
	long alloc_size = myDims[0] * myDims[1] * myDims[2]; //how many elements to fetch

		
	//printDataSequentiallyOnAllProcs(global_rank, global_size, data_buffer, alloc_size);
	
	
	nc_err = ncmpi_begin_indep_data(ncid);
	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (B)\n", global_rank, ncmpi_strerror (nc_err));
	
	int xOffset = dim_Sizes[1] * dim_Sizes[2] * dim_Mins[0];
	int yOffset = dim_Sizes[2] * dim_Mins[1];
	int zOffset = dim_Mins[2];
	
	int total_offset = (xOffset + yOffset + zOffset) * INPUT_DATA_WIDTH;
	//accurate total variable offset to START from, in the input data buffer
	//dif. for every process, b/c of the different dim_mins (starting positions)
	
	double testdub[alloc_size];
	int vars_read = 0;
	int ix, iy, iz;
	int cur_offset;
	
	//printf("%d: off: %d\n", global_rank, total_offset);

	for(n = 0; n < num_vars; n++, vars_read = 0)
	{
		for(ix = 0; ix < myDims[0]; ix++)
		{
			for(iy = 0; iy < myDims[1]; iy++)
			{
				for(iz = 0; iz < myDims[2]; iz++)
				{
					cur_offset = total_offset + (INPUT_DATA_WIDTH * (
													iz + 
													(iy * dim_Sizes[2]) + 
													(ix * dim_Sizes[1] * dim_Sizes[2]) ) );
													
					testdub[vars_read] = ((double*) inputData)[cur_offset + 3 + n];
					//printf("%0.0f, ", testdub[vars_read]);
					//testdub[vars_read] = 
					vars_read++;
				}
			}
		}
		nc_err = ncmpi_put_vara_double(ncid, var_Ids[n], start, readsize, testdub);
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (C)\n", global_rank, ncmpi_strerror (nc_err));
	}
	
	nc_err = ncmpi_end_indep_data(ncid);
	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (D)\n", global_rank, ncmpi_strerror (nc_err));
	
	nc_err = ncmpi_close(ncid);
	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (E)\n", global_rank, ncmpi_strerror (nc_err));
	

	if(global_rank != ROOT_PROCESS)
		free(nc5_file_name);
		

		
	free(dimStrList);
	free(varStrList);
	free(dimIndexStrList);
	free(varIndexStrList);	
	
	free(dim_Ids);
	free(var_Ids);
	
	free(dim_Mins);
	
	free(dim_Sizes);
	free(var_Dim_Sizes);

	return;
}

void* getTestDataBuffer(int vars, int x, int y, int z)
{
	const int totalSize = vars * x * y * z;

	int i, j, k;
	int n = 0;

	void* data = malloc(sizeof(double) * totalSize);

	for(i = 0; i < x; i++)
	{
		for(j = 0; j < y; j++)
		{
			for(k = 0; k < z; k++)
			{
				((double*)data)[n] = i;		//x
				((double*)data)[n+1] = j;	//y
				((double*)data)[n+2] = k;	//z
				
				((double*)data)[n+3] = n+3;
				((double*)data)[n+4] = n+4;
				n+=5;
			}
		}
	}
	return data;
}

void fillPriCoordPairBuffer(int dimStart, int dimEnd, int* buffer)
{	
	if(dimStart != 0)
		buffer[0] = rand() % dimStart; //rand start coord in the range of the region
	else
		buffer[0] = rand() % (dimStart + 1);
	
	int range = dimEnd - buffer[0]; //max possible values for random size
	
	if(range != 0)
		buffer[1] = buffer[0] + (rand() % range);
	else
		buffer[1] = buffer[0];
}

void printAllPriStats(int rank, pri_region_link* curPtr)
{
	printf("Rank %d: Tag %c: Pri %d: (%d, %d, %d) to (%d, %d, %d)\n", 
		rank, curPtr->tag, curPtr->priority_level, 
		curPtr->start[0], curPtr->start[1], curPtr->start[2], 
		curPtr->end[0], curPtr->end[1], curPtr->end[2]);
	if(curPtr->next != NULL)
		printAllPriStats(rank, curPtr->next); //recursively print the other linked regions
	else
		return;
}

char isPrime(long num)
{
	int i;
	long sqrt_num = ceil(sqrt(num)); 
	if(num <= 1) return 0;
	if(num % 2 == 0) return 0;
	
	for(i = 3; i < sqrt_num; i++)
	{
		if(num % i == 0)
			return 0;
	}
	return 1;
}
void printDataSequentiallyOnAllProcs(int rank, int global_size, void* data, long data_size)
{
	int dummy_var, n;
	if(rank == 0)
	{
		for(n = 0; n < data_size; n++)
		{
			printf("%.0f ", ((float*)data)[n]);
		}
		printf("\n");
		MPI_Send(&dummy_var, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
	}
	else if(rank == (global_size - 1))
	{
		MPI_Recv(&dummy_var, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		for(n = 0; n < data_size; n++)
		{
			printf("%.0f ", ((float*)data)[n]);
		}
		printf("\n");
	}
	else
	{
		MPI_Recv(&dummy_var, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		for(n = 0; n < data_size; n++)
		{
			printf("%.0f ", ((float*)data)[n]);
		}
		printf("\n");
		MPI_Send(&dummy_var, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
	}
}
char accumulateTagThroughGroup(char start_tag, int pri_regions_this_proc, MPI_Comm group, int group_rank, int group_size)
{
	char temp;
	char tempAlt;
	if(group_rank == 0)
	{
		temp = start_tag + pri_regions_this_proc;
		MPI_Send(&temp, 1, MPI_CHAR, group_rank + 1, 0, group);
		return start_tag;
	}
	else if(group_rank == (group_size - 1))
	{
		MPI_Recv(&temp, 1, MPI_CHAR, group_rank - 1, 0, group, MPI_STATUS_IGNORE);
		return temp;
	}
	else
	{
		MPI_Recv(&temp, 1, MPI_CHAR, group_rank - 1, 0, group, MPI_STATUS_IGNORE);
		tempAlt = temp + pri_regions_this_proc;
		MPI_Send(&tempAlt, 1, MPI_CHAR, group_rank + 1, 0, group);
		return temp;
	}
}
static void* allocAndFetchDataByTypeAndNum(int ncid, int varid, MPI_Offset* start, MPI_Offset* readsize, nc_type* var_type, long num)
{
	int n;
	int nc_err = 0;
	void* buffer = NULL;

	switch(*var_type)
	{
		/*case NC_BYTE:
			buffer = malloc(num);
			nc_err = ncmpi_get_vara_ubyte_all(ncid, varid, start, readsize, buffer);
			break;*/
		case NC_CHAR:
			buffer = malloc(sizeof(char) * num);
			nc_err = ncmpi_get_vara_schar_all(ncid, varid, start, readsize, buffer);
			break;
		case NC_SHORT:
			buffer = malloc(sizeof(short) * num);
			nc_err = ncmpi_get_vara_short_all(ncid, varid, start, readsize, buffer);
			break;
		case NC_LONG:
			buffer = malloc(sizeof(long) * num);
			nc_err = ncmpi_get_vara_long_all(ncid, varid, start, readsize, buffer);
			break;
		case NC_DOUBLE:
			buffer = malloc(sizeof(double) * num);
			nc_err = ncmpi_get_vara_double_all(ncid, varid, start, readsize, buffer);
			break;
		case NC_FLOAT:
			buffer = malloc(sizeof(float) * num);
			nc_err = ncmpi_get_vara_float_all(ncid, varid, start, readsize, buffer);
			//for(n=0; n < num; n++)printf("%f ", ((float*)buffer)[n]);
			//printf("\n");
			break;		
	}
	if (nc_err != NC_NOERR) fprintf(stderr, "%s (A)\n", ncmpi_strerror (nc_err));
	return buffer;
}
int getRandNumPriorityRegions(int max_regions, float percent_0_bias)
{
	int temp = rand() % max_regions;
	
	//printf("temp: %d, float: %f\n", temp, max_regions - ((float)max_regions * percent_0_bias));
	
	if(temp > (max_regions - ((float)max_regions * percent_0_bias)))
	{
		return 0;
	} 
	return temp;
}
static int* allocDimensions(int x, int y, int z)
{
	int* ret = malloc(sizeof(int) * 3);
	ret[0] = x;
	ret[1] = y;
	ret[2] = z;
	return ret;
}
float getMiddle(int start, int range)
{
	return start + ((float)range / 2.0);
}
float getFloatIntDistance(float* A, int* B)
{
	return sqrt( pow(A[0] - B[0], 2) + pow(A[1] - B[1], 2) + pow(A[2] - B[2], 2) );
}
float getDistance(float* A, float* B)
{
	return sqrt( pow(A[0] - B[0], 2) + pow(A[1] - B[1], 2) + pow(A[2] - B[2], 2) );
}

char isPriorityInMyRegion(int* pri_start, int* my_start)
{
	int i = 0;
	int my_ranges[3], pri_ranges[3];
	float my_middles[3], pri_middles[3];
	
	float distance, min_dist_to_intersect;
	
	for(i = 0; i < 3; i++)
	{
		my_ranges[i] = my_start[i + 3] - my_start[i];
		pri_ranges[i] = pri_start[i + 3] - pri_start[i];
	
		my_middles[i] = getMiddle(my_start[i], my_ranges[i]);
		pri_middles[i] = getMiddle(pri_start[i], pri_ranges[i]);	
	}
	float my_dist_to_vertex = getFloatIntDistance(my_middles, my_start);
	float pri_dist_to_vertex = getFloatIntDistance(pri_middles, pri_start);
	//distance from the center of my cube to any vertex
	
	min_dist_to_intersect = my_dist_to_vertex + pri_dist_to_vertex;
	
	distance = getDistance (my_middles, pri_middles);
	
	//printf("dist: %f, min: %f\n", distance, min_dist_to_intersect);
	
	return distance < min_dist_to_intersect;
	//printf("no intersect!\n");
	//printf("sx: %d, sy: %d, sz: %d, ex: %d, ey: %d, ez: %d\n",
			//my_start[0], my_start[1], my_start[2], my_start[3], my_start[4], my_start[5]);
	//printf("mx: %f, my: %f, mz: %f\n\n", my_middle[0], my_middle[1], my_middle[2]);
}

char largestIndexOfThree(int a, int b, int c)
{
	if(a >= b)
	{
		if(a >= c)
			return 0;
		else 
			return 2;
	}
	else if(a >= c)
	{
		if(a >= b)
			return 0;
		else 
			return 1;
	}
	else
	{
		if(b >= c)
			return 1;
		else
			return 2;
	}
}
int* testModulusWithDims(int mod, int x, int y, int z, int procs)
{
	switch(largestIndexOfThree(x, y, z))
	{
		case 0:
			if(x % mod == 0)
				return allocAndReturnBestDims(x / mod, y, z, procs / mod);
			else if(y % mod == 0)
				return allocAndReturnBestDims(x, y / mod, z, procs / mod);
			else if(z % mod == 0)
				return allocAndReturnBestDims(x, y, z / mod, procs / mod);
		case 1:
			if(x % mod == 0)
				return allocAndReturnBestDims(x / mod, y, z, procs / mod);
			else if(y % mod == 0)
				return allocAndReturnBestDims(x, y / mod, z, procs / mod);
			else if(z % mod == 0)
				return allocAndReturnBestDims(x, y, z / mod, procs / mod);
		case 2:
			if(x % mod == 0)
				return allocAndReturnBestDims(x / mod, y, z, procs / mod);
			else if(y % mod == 0)
				return allocAndReturnBestDims(x, y / mod, z, procs / mod);
			else if(z % mod == 0)
				return allocAndReturnBestDims(x, y, z / mod, procs / mod);
		default: 
			printf("Couldn't split the dims, but procs remain!\n");
			return NULL;
	}
}

static int* allocAndReturnBestDims(int x, int y, int z, long num_procs)
{
	if(num_procs == 1)
	{
		return allocDimensions(x, y, z);
	}
	else if(isPrime(num_procs))
	{	
		if(z % num_procs == 0)
			return allocDimensions(x, y, z / num_procs);
		else if(y % num_procs == 0)
			return allocDimensions(x, y / num_procs, z);
		else if(x % num_procs == 0)
			return allocDimensions(x / num_procs, y, z);
		else
		{
			printf("COULDN'T SPLIT\n");
			return NULL;
		}
	}
	else if(num_procs % 2 == 0)
	{
		testModulusWithDims(2, x, y, z, num_procs);
	}
	else if(num_procs % 3 == 0)
	{
		testModulusWithDims(3, x, y, z, num_procs);
	}
	
}
char isEquallyDivisibleVolume(long volume, long num_procs)
{
	return (volume % num_procs == 0);
}
char* removeQuotesOnString(char* str)
{
	int length = strlen(str);
	char* ret = &str[1];
	ret[length-2] = '\0';
	//printf("%s\n", ret);
	return ret;
}
