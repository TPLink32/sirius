#ifndef _GTS_ANALYIS_H_
#define _GTS_ANALYSIS_H_

#include <stdint.h>
#include "gts_particle.h"

#if USE_EVPATH
#include "evpath.h"
#endif

#define PG_NUM (20*1024*1024)

typedef struct _gts_local {
  long int mi_cnt;
  double mi_min_x;
  double mi_max_x;
  double mi_sqr_sum_x;
  double mi_sum_x;
  double mi_min_y;
  double mi_max_y;
  double mi_sqr_sum_y;
  double mi_sum_y;
  double mi_min_z;
  double mi_max_z;
  double mi_sqr_sum_z;
  double mi_sum_z;
  double mi_min_v1;
  double mi_max_v1;
  double mi_sqr_sum_v1;
  double mi_sum_v1;
  double mi_min_w;
  double mi_max_w;
  double mi_sqr_sum_w;
  double mi_sum_w;
  double mi_min_v2;
  double mi_max_v2;
  double mi_sqr_sum_v2;
  double mi_sum_v2;


  long int me_cnt;
  double me_min_x;
  double me_max_x;
  double me_sqr_sum_x;
  double me_sum_x;
  double me_min_y;
  double me_max_y;
  double me_sqr_sum_y;
  double me_sum_y;
  double me_min_z;
  double me_max_z;
  double me_sqr_sum_z;
  double me_sum_z;
  double me_min_v1;
  double me_max_v1;
  double me_sqr_sum_v1;
  double me_sum_v1;
  double me_min_w;
  double me_max_w;
  double me_sqr_sum_w;
  double me_sum_w;
  double me_min_v2;
  double me_max_v2;
  double me_sqr_sum_v2;
  double me_sum_v2;
} gts_local, *gts_local_t;

typedef struct _gts_global {
  long int mi_gcnt;
  double mi_gmin_x;
  double mi_gmax_x;
  double mi_sqr_sum_x;
  double mi_sum_sqr_x;
  double mi_gmin_y;
  double mi_gmax_y;
  double mi_sqr_sum_y;
  double mi_sum_sqr_y;
  double mi_gmin_z;
  double mi_gmax_z;
  double mi_sqr_sum_z;
  double mi_sum_sqr_z;
  double mi_gmin_v1;
  double mi_gmax_v1;
  double mi_sqr_sum_v1;
  double mi_sum_sqr_v1;
  double mi_gmin_w;
  double mi_gmax_w;
  double mi_sqr_sum_w;
  double mi_sum_sqr_w;
  double mi_gmin_v2;
  double mi_gmax_v2;
  double mi_sqr_sum_v2;
  double mi_sum_sqr_v2;

  long int me_gcnt;
  double me_gmin_x;
  double me_gmax_x;
  double me_sqr_sum_x;
  double me_sum_sqr_x;
  double me_gmin_y;
  double me_gmax_y;
  double me_sqr_sum_y;
  double me_sum_sqr_y;
  double me_gmin_z;
  double me_gmax_z;
  double me_sqr_sum_z;
  double me_sum_sqr_z;
  double me_gmin_v1;
  double me_gmax_v1;
  double me_sqr_sum_v1;
  double me_sum_sqr_v1;
  double me_gmin_w;
  double me_gmax_w;
  double me_sqr_sum_w;
  double me_sum_sqr_w;
  double me_gmin_v2;
  double me_gmax_v2;
  double me_sqr_sum_v2;
  double me_sum_sqr_v2;
} gts_global, *gts_global_t;

#if USE_EVPATH
static FMField gts_local_field_list[] =
{
    {"mi_cnt", "integer", sizeof(int), FMOffset(gts_local_t, mi_cnt)},
    {"mi_min_x", "float", sizeof(float), FMOffset(gts_local_t, mi_min_x)},
    {"mi_max_x", "float", sizeof(float), FMOffset(gts_local_t, mi_max_x)},
    {"mi_sqr_sum_x", "float", sizeof(float), FMOffset(gts_local_t, mi_sqr_sum_x)},
    {"mi_sum_x", "float", sizeof(float), FMOffset(gts_local_t, mi_sum_x)},
    {"mi_min_y", "float", sizeof(float), FMOffset(gts_local_t, mi_min_y)},
    {"mi_max_y", "float", sizeof(float), FMOffset(gts_local_t, mi_max_y)},
    {"mi_sqr_sum_y", "float", sizeof(float), FMOffset(gts_local_t, mi_sqr_sum_y)},
    {"mi_sum_y", "float", sizeof(float), FMOffset(gts_local_t, mi_sum_y)},
    {"mi_min_z", "float", sizeof(float), FMOffset(gts_local_t, mi_min_z)},
    {"mi_max_z", "float", sizeof(float), FMOffset(gts_local_t, mi_max_z)},
    {"mi_sqr_sum_z", "float", sizeof(float), FMOffset(gts_local_t, mi_sqr_sum_z)},
    {"mi_sum_z", "float", sizeof(float), FMOffset(gts_local_t, mi_sum_z)},
    {"me_cnt", "integer", sizeof(int), FMOffset(gts_local_t, me_cnt)},
    {"me_min_x", "float", sizeof(float), FMOffset(gts_local_t, me_min_x)},
    {"me_max_x", "float", sizeof(float), FMOffset(gts_local_t, me_max_x)},
    {"me_sqr_sum_x", "float", sizeof(float), FMOffset(gts_local_t, me_sqr_sum_x)},
    {"me_sum_x", "float", sizeof(float), FMOffset(gts_local_t, me_sum_x)},
    {"me_min_y", "float", sizeof(float), FMOffset(gts_local_t, me_min_y)},
    {"me_max_y", "float", sizeof(float), FMOffset(gts_local_t, me_max_y)},
    {"me_sqr_sum_y", "float", sizeof(float), FMOffset(gts_local_t, me_sqr_sum_y)},
    {"me_sum_y", "float", sizeof(float), FMOffset(gts_local_t, me_sum_y)},
    {"me_min_z", "float", sizeof(float), FMOffset(gts_local_t, me_min_z)},
    {"me_max_z", "float", sizeof(float), FMOffset(gts_local_t, me_max_z)},
    {"me_sqr_sum_z", "float", sizeof(float), FMOffset(gts_local_t, me_sqr_sum_z)},
    {"me_sum_z", "float", sizeof(float), FMOffset(gts_local_t, me_sum_z)},
    {NULL, NULL, 0, 0}
};

typedef struct _filtered_particles {
    int num_mi;
    double *pgmi_qualify;
    gts_local *gts_local_features;
} filtered_particles, *filtered_particles_ptr;

static FMField filtered_particles_field_list[] =
{
    {"num_mi", "integer", sizeof(int), FMOffset(filtered_particles_ptr, num_mi)},
    {"pgmi_quality", "float[7][num_mi]", sizeof(double), FMOffset(filtered_particles_ptr, pgmi_qualify)},
    {"gts_local_features", "gts_local", sizeof(gts_local), FMOffset(filtered_particles_ptr, gts_local_features)},
    {NULL, NULL, 0, 0}
};

static FMStructDescRec filtered_particles_format_list[] =
{
    {"simple", filtered_particles_field_list, sizeof(filtered_particles), NULL},
    {NULL, NULL}
};
#endif

//int Analyp(double *, int, double *, int, double *, int *, gts_local *);
int Analyp(double *zion, int mi, gts_local *_pgl);

int Scanfilter(double *zion, int mi, double *_filter_results, long int *_count, double* _lower_limit, double* _upper_limit);

int Analyp_combine(double *zion, int32_t mi, gts_local *_pgl, int32_t first_pg);

int Scanfilter_combine(double *zion, int32_t mi, double *_filter_results, int64_t *_count, double* _lower_limit, double* _upper_limit);

#endif

