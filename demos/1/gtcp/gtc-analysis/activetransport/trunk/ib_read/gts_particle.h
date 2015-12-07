/*
 * GTS particle data format definition
 * 
 * There are two particle arrays in the GTS output data structure:
 * - one is for zion particle and of total size 7 x mi_total.
 * - the other is for zeon particle and of total size 7 x me_total.
 *
 * zion is distributed among processes along the second dimension,  
 * so each process' output contains a local zion array and its size
 * is 7 x mi. Its offset in global array is specified by mi_offset.
 * 
 * The situation for zeon array is similar.
 *
 */

#ifndef _GTS_PARTICLE_H_
#define _GTS_PARTICLE_H_

#include "evpath.h"

/* 
 * structure of the output chunk from one GTS simulation process
 */
typedef struct _gts_particles {
  int istep;          // timestep, start from 1
  int mype;           // mpi rank of this gts process 
  int numberpe;       // total number of gts processes
  int mi;             // number of zion particles
  long int mi_total;  // total number of zion particles
  long int mi_offset; // offset of zion array in global zion array
  double ntracer;     // ignore, no use
  double etracer_i0;  // ignore, no use
  double misum;       // equal to mi_total, just in floating point
  double *zion;       // particle array for zions of size 7 x mi
  double ntracer_e;   // ignore, no use
  double etracer_e0;  // ignore, no use
  double mesum;       // equal to me_total, just in floating point
  int me;          // number of zeon particles
  long int me_total;    // total number of zeon particles
  long int me_offset;   // offset of zeon array in global zeon array
  double *zeon;       // particle array for zeons of size 7 x me
} gts_particles, *gts_particles_t;

static  FMField gts_particles_field_list[] =
{
    {"istep", "integer", sizeof(int), FMOffset(gts_particles_t, istep)},
    {"mype", "integer", sizeof(int), FMOffset(gts_particles_t, mype)},
    {"numberpe", "integer", sizeof(int), FMOffset(gts_particles_t, numberpe)},
    {"mi", "integer", sizeof(int), FMOffset(gts_particles_t, mi)},
    {"mi_total", "integer", sizeof(long int), FMOffset(gts_particles_t, mi_total)},
    {"mi_offset", "integer", sizeof(long int), FMOffset(gts_particles_t, mi_offset)},
    {"ntracer", "float", sizeof(double), FMOffset(gts_particles_t, ntracer)},
    {"etracer_i0", "float", sizeof(double), FMOffset(gts_particles_t, etracer_i0)},
    {"misum", "float", sizeof(double), FMOffset(gts_particles_t, misum)},
    {"zion", "float[7][mi]", sizeof(double), FMOffset(gts_particles_t, zion)},
    {"ntracer_e", "float", sizeof(double), FMOffset(gts_particles_t, ntracer_e)},
    {"etracer_e0", "float", sizeof(double), FMOffset(gts_particles_t, etracer_e0)},
    {"mesum", "float", sizeof(double), FMOffset(gts_particles_t, mesum)},
    {"me", "integer", sizeof(int), FMOffset(gts_particles_t, me)},
    {"me_total", "integer", sizeof(long int), FMOffset(gts_particles_t, me_total)},
    {"me_offset", "integer", sizeof(long int), FMOffset(gts_particles_t, me_offset)},
    {"zeon", "float[7][me]", sizeof(double), FMOffset(gts_particles_t, zeon)},
    {NULL, NULL, 0, 0}
};

static FMFormatRec gts_particles_format_list[] =
{
    {"gts_particles", gts_particles_field_list, sizeof(gts_particles), NULL},
    {NULL, NULL, 0, NULL}
};


static  FMField hack_gts_particles_field_list[] =
{
    {"istep", "integer", 4, 0},
    {"mype", "integer", 4, 8},
    {"numberpe", "integer", 4, 16},
    {"mi", "integer", 4, 24},
    {"mi_total", "integer", 8, 32},
    {"mi_offset", "integer", 8, 40},
    {"ntracer", "float", 8, 48},
    {"etracer_i0", "float", 8, 56},
    {"misum", "float", 8, 64},
    {"zion", "float[7][mi]", 8, 72},
    {"ntracer_e", "float", 8, 80},
    {"etracer_e0", "float", 8, 88},
    {"mesum", "float", 8, 96},
    {"me", "integer", 4, 104},
    {"me_total", "integer", 8, 112},
    {"me_offset", "integer", 8, 120},
    {"zeon", "float[7][me]", 8, 128},
    {NULL, NULL, 0, 0}
};

static FMFormatRec hack_gts_particles_format_list[] =
{
    {"restart", hack_gts_particles_field_list, 136, NULL},
    {NULL, NULL, 0, NULL}
};


#endif
