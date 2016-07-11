#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>

#include <pnetcdf.h>

#include "jsonparser.h"

//compile w/ "mpicc groupTest.c -lm -lpnetcdf -o "exe_name" "
//run w/ "mpirun -np x ./group" where x is # of processes

#define JSON_FILE_NAME "paramstest.json"

#define FILENAME_CHAR_LENGTH 50

#define NUM_DIMS 4

#define ROOT_PROCESS 0
#define NUM_COLORS 10
#define NUM_VARS 4

#define MAX_PRI_REGIONS 10
#define PERCENT_0_PRI_BIAS 0.80	//the % "skew" towards having exactly 0 priority regions locally

#define FLOAT_TYPE 'f'
#define INT_TYPE 'i'
#define DOUBLE_TYPE 'd'
#define LONG_TYPE 'l'

void printUsage();
char* removeQuotesOnString(char* str);
char isPrime(long x);
char isEquallyDivisibleVolume(long volume, long num_procs);
char largestIndexOfThree(int a, int b, int c);
int* returnBestDims(int x, int y, int z, long num_procs);
int* allocDimensions(int x, int y, int z);
char isPriorityInMyRegion(int* priStart, int* myStart);
int getRandNumPriorityRegions(int max_regions, float percent_0_bias);
void* allocAndFetchDataByTypeAndNum(int ncid, int varid, MPI_Offset* start, MPI_Offset* readsize, nc_type* var_type, long num);
void printDataSequentiallyOnAllProcs(int rank, int global_size, void* data, long data_size);

static FILE* LOG_FILE = NULL;

int main(int argc, char** argv)
{	
	double start_time, end_time;
	
	char** dimStrList = malloc(sizeof(char*) * NUM_DIMS);
	dimStrList[0] = "level";
	dimStrList[1] = "latitude";
	dimStrList[2] = "longitude";
	dimStrList[3] = "time";
	
	char** varStrList = malloc(sizeof(char*) * NUM_DIMS);
	varStrList[0] = "pressure";
	varStrList[1] = "temperature";
	varStrList[2] = "latitude";
	varStrList[3] = "longitude";

	//z,y,z,q respectively
	int dimIds[NUM_DIMS];
	int dimMins[NUM_DIMS]; //offsets for local
	MPI_Offset ns[NUM_DIMS];

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
	
	//LOG_FILE = fopen("debugLog.txt", "w");
	nc_err = MPI_Init(&argc, &argv);

	nc_err = MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);
	nc_err = MPI_Comm_size(MPI_COMM_WORLD, &global_size);	
	
	json_object** json_obj_arr_ptr = NULL;
	json_object** global_params_obj = NULL;
	//json_object** temp_priority_obj = NULL;
	char* nc5_file_name = NULL;

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
				nc5_file_name = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "fileReadName"));
				//printf("got globals!\n");
			}
			/*else 
			{
				for(n = 0; n < NUM_VARS; n++)
				{
					if(strcmp(current_obj_id, varStrList[n]) == 0)
					{
						//printf("found var: %d\n", n);
						
						variable_spec_arr[n].name = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "name"));
						variable_spec_arr[n].units = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "units"));
						
						char* tempType = removeQuotesOnString(getValueAsStringFromObjAndKey(temp, "size"));
						if(strcmp(tempType, "float") == 0)
						{
							variable_spec_arr[n].varType = FLOAT_TYPE;
						}
						else if(strcmp(tempType, "double") == 0)
						{
							variable_spec_arr[n].varType = DOUBLE_TYPE;
						}
						else if(strcmp(tempType, "int") == 0)
						{
							variable_spec_arr[n].varType = INT_TYPE;
						}
						else if(strcmp(tempType, "long") == 0)
						{
							variable_spec_arr[n].varType = LONG_TYPE;
						}
						
						//printf("name: %s, units: %s, type: %c\n", 
							//variable_spec_arr[n].name, variable_spec_arr[n].units, variable_spec_arr[n].varType );
					}
				}
			}*/
			/*else if(strcmp(current_obj_id, "\"priority\"") == 0)
			{
				temp_priority_obj = &temp;
				
				priority_2D_coord_arr[cur_priority_n] = getValueAsIntegerFromObjAndKey(temp, "startx");
				priority_2D_coord_arr[cur_priority_n+1] = getValueAsIntegerFromObjAndKey(temp, "starty");
				priority_2D_coord_arr[cur_priority_n+2] = getValueAsIntegerFromObjAndKey(temp, "startz");
				
				priority_2D_coord_arr[cur_priority_n+3] = getValueAsIntegerFromObjAndKey(temp, "endx");
				priority_2D_coord_arr[cur_priority_n+4] = getValueAsIntegerFromObjAndKey(temp, "endy");
				priority_2D_coord_arr[cur_priority_n+5] = getValueAsIntegerFromObjAndKey(temp, "endz");
				
				cur_priority_n++;
			
				//printf("got priority!\n");
			}*/
			i++;
			temp = json_obj_arr_ptr[i];
			//printf("%li\n", (long) temp);
		}
	}
	else
	{
		nc5_file_name = malloc(sizeof(char) * FILENAME_CHAR_LENGTH);
	}
	
	
	MPI_Bcast(nc5_file_name, FILENAME_CHAR_LENGTH, MPI_BYTE, ROOT_PROCESS, MPI_COMM_WORLD);
	//MPI_Bcast(&cur_priority_n, 1, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);

	//MPI_Bcast(priority_2D_coord_arr, 6 * cur_priority_n, MPI_INT, ROOT_PROCESS, MPI_COMM_WORLD);
	
	//now all processes have the start and end coords of a priority region

	nc_err = ncmpi_open(MPI_COMM_WORLD, nc5_file_name, NC_NOWRITE|NC_64BIT_DATA, MPI_INFO_NULL, &ncid);
	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (1)\n", global_rank, ncmpi_strerror (nc_err)); 
	
	for(i = 0; i < NUM_DIMS; i++)
	{
		nc_err = ncmpi_inq_dimid(ncid, dimStrList[i], &dimIds[i]);
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (%d)\n", global_rank, ncmpi_strerror (nc_err), (i*2) + 2);
		nc_err = ncmpi_inq_dim(ncid, dimIds[i], dim_name, &ns[i]);
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (%d)\n", global_rank, ncmpi_strerror (nc_err), (i*2) + 3);
	}
	//printf("NS: %d, %d, %d, %d\n", (int)ns[0], (int)ns[1], (int)ns[2], (int)ns[3]);
	
	long volume = ns[0] * ns[1] * ns[2];
	//not multiplying by ns[3], as this is assumed to be the time dimension- handled separately
	
	int* myDims;
	if(isEquallyDivisibleVolume(volume, global_size))
	{
		myDims = returnBestDims((int)ns[0],(int)ns[1],(int)ns[2], global_size);
		
		if(myDims == NULL)
		{
			printf("couldn't find ideal coordinates");
			return;
		}
		else
		{
			//printf("dims: %d, %d, %d\n", myDims[0], myDims[1], myDims[2]);
			//int* test = returnBestDims(10, 10, 10, 10);
			//printf("dims: %d, %d, %d\n", test[0], test[1], test[2]);
			//free(test);
		}
	}
	else
	{
		printf("can't evenly divide the volume, exiting");
		return;
	}

		
	n_procs[0] = ns[0] / myDims[0];
	n_procs[1] = ns[1] / myDims[1];
	n_procs[2] = ns[2] / myDims[2];
		
	dimMins[0] = (global_rank % n_procs[0]) * myDims[0];
	dimMins[1] = ((global_rank / n_procs[0]) % n_procs[1]) * myDims[1];
	dimMins[2] = global_rank / (n_procs[0] * n_procs[1]) * myDims[2];
		
	readsize[0] = 1; //time dimension	
	for(i = 0; i < 3; i++)
	{
		if(dimMins[i] + myDims[i] > ns[i])
		{
			readsize[i+1] = ns[i] - dimMins[i];
			printf("%d in %d plane shortened to %d\n", (dimMins[i] + myDims[i]), i, (int)readsize[i+1]);
		}
		else
		{
			readsize[i+1] = myDims[i];
		}
	}
	
	/*int rangeArr[6];
	rangeArr[0] = dimMins[0];
	rangeArr[1] = dimMins[1];
	rangeArr[2] = dimMins[2];
	rangeArr[3] = dimMins[0] + myDims[0];
	rangeArr[4] = dimMins[1] + myDims[1];
	rangeArr[5] = dimMins[2] + myDims[2];*/

	//isPriorityInMyRegion(priority_2D_coord_arr, rangeArr);
	
	int* pri_region_coords[rand_pri_region_count];
	
	for(i = 0; i < rand_pri_region_count; i++)
	{
		pri_region_coords[i] = malloc(sizeof(int) * 6);
		for(n = 0; n < 3; n++)
		{
			pri_region_coords[i][n] = dimMins[i] + (rand() % myDims[i]) ;
			//rand offset from start of local space
			
			//printf("%d start: %d", global_rank, pri_region_coords[i][n]);
			
			int maxLength = (dimMins[n] + myDims[n]) - pri_region_coords[i][n];
			if(maxLength == 0) 
				pri_region_coords[i][n+3] = pri_region_coords[i][n];
			else
				pri_region_coords[i][n+3] = pri_region_coords[i][n] + (rand() % maxLength);
			//selects a random number not greater than the distance between the random start pos, 
			//and the end of the local range
			
			//printf(" end: %d\n", pri_region_coords[i][n+3]);
		}
	}

	start[0] = 0; //time dimension
	start[1] = dimMins[0];
	start[2] = dimMins[1];
	start[3] = dimMins[2];
	
	//printf("%d has start: (%d,%d,%d) and size: (%d,%d,%d)\n", 
			//global_rank, dimMins[0], dimMins[1], dimMins[2], myDims[0], myDims[1], myDims[2]);

	//long total_var_count = readsize[1] * readsize[2] * readsize[3]; //not worrying about time dimension at the moment
	int* num_dims_per_var = malloc(sizeof(int));
	int* this_var_dim_ids = NULL;
	long alloc_size = 1; //how many elements to fetch
	nc_type* var_type = malloc(sizeof(nc_type));
	void* data_buffer;
	//printf("readsizes: %d %d %d\n", (int)start[0], (int)start[1], (int)start[2]);
	
	for(i = 0; i < 1; i++, alloc_size = 1)
	{
		nc_err = ncmpi_inq_varid(ncid, varStrList[i], &grav_x_c_varid);
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (6)\n", global_rank, ncmpi_strerror (nc_err));
	
		
		nc_err = ncmpi_inq_varndims(ncid, grav_x_c_varid, num_dims_per_var);	
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (7)\n", global_rank, ncmpi_strerror (nc_err));
		
		this_var_dim_ids = malloc(sizeof(int) * (*num_dims_per_var));
		
		nc_err = ncmpi_inq_vardimid(ncid, grav_x_c_varid, this_var_dim_ids);	
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (8)\n", global_rank, ncmpi_strerror (nc_err));
		//gets the ids of all dimensions for the current variable
		
		
		for(n = 0; n < *num_dims_per_var; n++) //go through the dimensions of the current var
		{
			for(w = 0; w < NUM_DIMS; w++)
			{
				if(dimIds[w] == this_var_dim_ids[n]) //on a match, determine the allocation size based
				{									 //on the dimensions already determined earlier
					alloc_size *= readsize[w];
					
				}
			}
			
		}
		//printf("%d alloc'ed %li\n", global_rank, alloc_size);
		nc_err = ncmpi_inq_vartype(ncid, grav_x_c_varid, var_type);	
		if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (9)\n", global_rank, ncmpi_strerror (nc_err));
		
		data_buffer = allocAndFetchDataByTypeAndNum(ncid, grav_x_c_varid, start, readsize, var_type, alloc_size);
		
		//MPI_Barrier(MPI_COMM_WORLD);
		
		printDataSequentiallyOnAllProcs(global_rank, global_size, data_buffer, alloc_size);
	
		//printf("%d: %s\n", global_rank, varStrList[i]);
		free(this_var_dim_ids);
	}
		
	nc_err = ncmpi_close(ncid);
	if (nc_err != NC_NOERR) fprintf(stderr, "%d: %s (B)\n", global_rank, ncmpi_strerror (nc_err));

	
	/*if(global_rank == (global_size - 1))
	{
		printf("RANK DATA: %d\n", global_rank);
		for(i = 0; i < total_var_count; i++)
		{
			printf("%d ", (int)grav_x_c[i]);
		}
	}*/
	

	/*int my_color = global_rank % NUM_COLORS;
	
	MPI_Comm local_comm;
	MPI_Comm_split(MPI_COMM_WORLD, my_color, global_rank, &local_comm);

	nc_err = MPI_Comm_rank(local_comm, &local_rank);
	nc_err = MPI_Comm_size(local_comm, &local_size);*/

	//MPI_Group world_group;
	//MPI_Comm_group(MPI_COMM_WORLD, &world_group);

	//printRanks(global_rank, local_rank);

	//MPI_Group_free(&world_group);

	//MPI_Comm_free(&local_comm);
	//freeObjArr(json_obj_arr_ptr);
	free(myDims);
	//free(grav_x_c);
	if(global_rank != ROOT_PROCESS)
		free(nc5_file_name);
		
	free(dimStrList);
	for(i = 0; i < rand_pri_region_count; i++)
	{
		free(pri_region_coords[i]);
	}
	free(num_dims_per_var);
	free(var_type);
	//free(startStr);
	//free(endStr);

	nc_err = MPI_Finalize();
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
void* allocAndFetchDataByTypeAndNum(int ncid, int varid, MPI_Offset* start, MPI_Offset* readsize, nc_type* var_type, long num)
{
	int n;
	int nc_err = 0;
	void* buffer = NULL;
	//MPI_Offset startAlt[4] = start;
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
int* allocDimensions(int x, int y, int z)
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
				return returnBestDims(x / mod, y, z, procs / mod);
			else if(y % mod == 0)
				return returnBestDims(x, y / mod, z, procs / mod);
			else if(z % mod == 0)
				return returnBestDims(x, y, z / mod, procs / mod);
		case 1:
			if(x % mod == 0)
				return returnBestDims(x / mod, y, z, procs / mod);
			else if(y % mod == 0)
				return returnBestDims(x, y / mod, z, procs / mod);
			else if(z % mod == 0)
				return returnBestDims(x, y, z / mod, procs / mod);
		case 2:
			if(x % mod == 0)
				return returnBestDims(x / mod, y, z, procs / mod);
			else if(y % mod == 0)
				return returnBestDims(x, y / mod, z, procs / mod);
			else if(z % mod == 0)
				return returnBestDims(x, y, z / mod, procs / mod);
		default: 
			printf("Couldn't split the dims, but procs remain!\n");
			return NULL;
	}
}

int* returnBestDims(int x, int y, int z, long num_procs)
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
