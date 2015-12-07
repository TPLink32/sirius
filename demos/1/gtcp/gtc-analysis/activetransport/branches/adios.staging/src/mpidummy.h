/* 
 * ADIOS is freely available under the terms of the BSD license described
 * in the COPYING file in the top level directory of this source distribution.
 *
 * Copyright (c) 2008 - 2009.  UT-BATTELLE, LLC. All rights reserved.
 */

#ifndef __MPI_DUMMY_H__
#define __MPI_DUMMY_H__

/*
   A dummy MPI 'implementation' for the BP READ API, to have an MPI-free version of the API
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>

typedef int MPI_Comm;
typedef uint64_t MPI_Status;
typedef int MPI_File;
typedef int MPI_Info;
typedef int MPI_Datatype;  /* Store the byte size of a type in such vars */
typedef uint64_t MPI_Offset;
typedef int MPI_Fint;

#define MPI_SUCCESS                 0
#define MPI_MAX_ERROR_STRING        512
#define MPI_MODE_RDONLY             O_RDONLY
#define MPI_SEEK_SET                SEEK_SET
#define MPI_SEEK_CUR                SEEK_CUR
#define MPI_SEEK_END                SEEK_END
#define MPI_BYTE                    1          /* I need the size of the type here */
#define MPI_INFO_NULL               NULL

#define MPI_COMM_WORLD              1

#define MPI_INT                     1

int MPI_Init(int *argc, char ***argv);
int MPI_Finalize();

int MPI_Barrier(MPI_Comm comm);
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm);

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm);
int MPI_Comm_rank(MPI_Comm comm, int *rank);
int MPI_Comm_size(MPI_Comm comm, int *size);
MPI_Comm MPI_Comm_f2c(MPI_Fint comm);

int MPI_File_open(MPI_Comm comm, char *filename, int amode, MPI_Info info, MPI_File *fh);
int MPI_File_close(MPI_File *fh);
int MPI_File_get_size(MPI_File fh, MPI_Offset *size);
int MPI_File_read(MPI_File fh, void *buf, int count, MPI_Datatype datatype, MPI_Status *status);
int MPI_File_seek(MPI_File fh, MPI_Offset offset, int whence);

int MPI_Get_count(MPI_Status *status, MPI_Datatype datatype, int *count);
int MPI_Error_string(int errorcode, char *string, int *resultlen);
int MPI_Comm_split ( MPI_Comm comm, int color, int key, MPI_Comm *comm_out );


#endif
