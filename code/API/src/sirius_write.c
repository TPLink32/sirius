#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mpi.h>
#include "sirius.h"

int main(int argc, char **argv)
{
    int i, j, k, l;
    int cube_dim[3] ;
    int npx, npy, npz, ndx, ndy, ndz, nx, ny, nz;
    int offx, offy, offz, posx, posy, posz;
    int offsets[3], lsize[3], gsize[3];
    uint64_t cube_start[3];
    uint64_t cube_count[3];
    double *ddata = NULL;
    double * A, * B, * C, * D, * E, * F, * G, * H, * I, * J;
    int rank;
    int nprocs;
    double start_time, end_time, t_time, sz, gps;
    int time;
    int status;
    char filename [256];

    struct SIRIUS_HANDLE sirius_write_handle;
    
    struct SIRIUS_WRITE_OPTIONS sirius_write_options_handle;
    
    MPI_Comm comm = MPI_COMM_WORLD;

    sprintf (filename, "%s.bp", argv [1]);

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    sirius_init (&sirius_write_options_handle, MPI_COMM_WORLD, "paramstest.json");
    
  //  printf("%d %d %d\n", sirius_write_handle.global_dimensions[0], sirius_write_handle.global_dimensions[1], sirius_write_handle.global_dimensions[2]);

    if (argc < 7)
    {
        if (rank == 0)
        {
            fprintf (stderr, "usage: %s filename <proc counts> <local sizes>\n", argv [0]);
        }
        MPI_Finalize ();
        return 1;
    }
   

    npx = atoi(argv[2]);
    npy = atoi(argv[3]);
    npz = atoi(argv[4]); //number of processes per dimension

    ndx = atoi(argv[5]);
    ndy = atoi(argv[6]);
    ndz = atoi(argv[7]); //local sizes

    nx = npx * ndx;
    ny = npy * ndy;
    nz = npz * ndz;

    posx = rank%npx;
    posy = (rank/npx) %npy;
    posz = rank/(npx*npy);
  
    offx = posx * ndx;
    offy = posy * ndy;
    offz = posz * ndz;

    ddata = (double *)malloc(sizeof(double)*ndx*ndy*ndz);

    for (i=0;i<ndx*ndy*ndz;i++)
    {
        ddata [i] = rank;
    }

    A = B = C = D = E = F = G = H = I = J = ddata;

    cube_start[0] = offx;
    cube_start[1] = offy;
    cube_start[2] = offz;
 
    cube_count[0] = ndx;
    cube_count[1] = ndy;
    cube_count[2] = ndz;
    // the number of procs must be divisible by 8

//printf ("%03d start(0) %3d start(1) %3d start(2) %3d size(0) %3d size(1) %3d size(2) %3d\n", rank, cube_start[0], cube_start[1], cube_start[2], cube_count[0], cube_count[1], cube_count[2]);

    // start the timing now.
    status = MPI_Barrier (MPI_COMM_WORLD);
    start_time = MPI_Wtime ();

/////////////////////////////
// start SIRIUS write test //
/////////////////////////////
    status = sirius_open_write (&sirius_write_handle, filename, &comm);

// put the rest of the calls in this block

	struct SIRIUS_VAR_HANDLE var1_handle;
	struct SIRIUS_VAR_HANDLE var2_handle;
	
	status = sirius_write(&sirius_write_handle, NULL, "temperature", ddata, &var1_handle);
	status = sirius_write(&sirius_write_handle, NULL, "pressure", ddata, &var2_handle);
	
	uint32_t num_points = 10;
	int num_dims = 3;
	
	uint32_t total_coord_values = num_points * num_dims;
	
	uint64_t* start = malloc(sizeof(uint64_t) * total_coord_values);
	uint64_t* end = malloc(sizeof(uint64_t) * total_coord_values);
	
	int q;
	for(q = 0; q < total_coord_values; q++)
	{
		start[q] = q;
		end[q] = q + 1;
	}
	
	status = sirius_write_priority_regions (&sirius_write_handle, &num_points, start, end, &var1_handle);
	status = sirius_write_priority_regions (&sirius_write_handle, &num_points, start, end, &var2_handle);

	//print_linked_priority_regions(var1_handle.var_name, var1_handle.priority_head);
	//print_linked_priority_regions(var2_handle.var_name, var2_handle.priority_head);
	

	//
	// function to return a var handle, just with the name of the var?
	// var handle should be stored on metadata side anyways...
	//

    status = sirius_close (&sirius_write_handle);
///////////////////////////
// end SIRIUS write test //
///////////////////////////
	
    status = MPI_Barrier (MPI_COMM_WORLD);

    end_time = MPI_Wtime (); 

    t_time = end_time - start_time;
    sz = (8.0 * 8.0 * nx * ny * ((double) nz)) / (1024.0 * 1024.0 * 1024.0);
    gps = sz / t_time;
    if (rank == 0)
        printf ("%s %d %d %d %d %lf %lf %lf\n", filename, nprocs, ndx, ndy, ndz, sz, t_time, gps);

    free (ddata);
    free(start);
    free(end);
    sirius_finalize (&sirius_write_options_handle, rank);
    MPI_Finalize ();

    return 0;
}


