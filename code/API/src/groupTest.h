#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>

#include <pnetcdf.h>

#define JSON_FILE_NAME "paramstest.json"

#define FILENAME_CHAR_LENGTH 50
#define DIM_CHAR_LENGTH 50
#define INPUT_DATA_WIDTH 5

#define ROOT_PROCESS 0

#define MAX_PRI_REGIONS 10
#define PERCENT_0_PRI_BIAS 0.80	//the % "skew" towards having exactly 0 priority regions locally
#define PRIORITY_COLOR 1
#define NO_PRIORITY_COLOR 0

#define NULL_TAG '\0'


typedef struct priorityRegion pri_region_link;
void mainFunc(void* data, long dataSize, int argc, char** argv);
//	main function for the entire process, gets called by API directly

void* getTestDataBuffer(int vars, int x, int y, int z);

char* removeQuotesOnString(char* str);
//	takes a string with prefix and suffix double quotes, and removes them

char isPrime(long x);
//	returns 1 if x is prime, and 0 if it is not

char isEquallyDivisibleVolume(long volume, long num_procs);
//	returns 1 if the volume can be evenly split by num_procs, and 0 if not

char largestIndexOfThree(int a, int b, int c);
//	returns the index of the largest value (0 = a, 1 = b, 2 = c)

static int* allocAndReturnBestDims(int x, int y, int z, long num_procs);
/*	Determines the most "cubic" split of coordinates (recursively), based on the 
  	number of procedures. 
 	For example, if the space is 10 x 15 x 20 (volume = 3000) with 30 processes
 	(100 volume per process), then instead of, say, slices of (1 x 20 x 5)
 	or something, the function would instead aim for (5 x 4 x 5)*/

static int* allocDimensions(int x, int y, int z);
//returns an allocated array of 3-d coordinates

char isPriorityInMyRegion(int* priStart, int* myStart);
/*	tests if a 3d cube (pristart) intersects another (mystart)
	pri and my are 6-element arrays of the form 
	[startx, starty, startz, endx, endy, endz] */

int getRandNumPriorityRegions(int max_regions, float percent_0_bias);
//	returns a random number of priority regions, based on the received % 
//	tendency towards 0 priority regions

static void* allocAndFetchDataByTypeAndNum(int ncid, int varid, 
									MPI_Offset* start, MPI_Offset* readsize, 
									nc_type* var_type, long num);
/*  reads data of some type from the file/var regarded by the ncid/varid, 
	allocates a buffer, and returns a void* to this data*/

void printDataSequentiallyOnAllProcs(int rank, int global_size, void* data, long data_size);
//	forces the processes to print their data IN ORDER to standard output

void fillPriCoordPairBuffer(int dimStart, int dimEnd, int* buffer);
//	gets random start coordinates and sizes for a priority region,
//	and fills the provided buffer with the start and end coordinates
// 	for a single dimension. EX: buffer = [startx, endx]

void printAllPriStats(int rank, pri_region_link* curPtr);
//	traverses the linked list of priority regions (recursively) and prints
//	their relevant info

char accumulateTagThroughGroup(char start_tag, int pri_regions_this_proc, MPI_Comm group, int group_rank, int group_size);
//	passes a char tag through all processes with priority regions, 
// 	to ensure that every region in the overall dataset gets a unique tag
// 	in the global space, and not just for each local process


