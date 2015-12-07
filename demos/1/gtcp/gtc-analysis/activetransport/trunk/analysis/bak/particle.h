/*
particles data
*/
#include "evpath.h"

typedef struct _particles_gtc {
    int mype;
    int nparam;
    int nspecies;
    int numberpe;
    int ntracke;
    int np_total;
    int my_offset;
    int ntracki;
    float *ptrackede;
    float *ptrackedi;
} particles_gtc, *particles_gtc_p;


static FMField particles_gtc_field_list[] =
{
    {"mype", "integer", sizeof(int), FMOffset(particles_gtc_p, mype)},
    {"nparam", "integer", sizeof(int), FMOffset(particles_gtc_p, nparam)},
    {"nspecies", "integer", sizeof(int), FMOffset(particles_gtc_p, nspecies)},
    {"numberpe", "integer", sizeof(int), FMOffset(particles_gtc_p, numberpe)},
    {"ntracke", "integer", sizeof(int), FMOffset(particles_gtc_p, ntracke)},
    {"np_total", "integer", sizeof(int), FMOffset(particles_gtc_p, np_total)},
    {"my_offset", "integer", sizeof(int), FMOffset(particles_gtc_p, my_offset)},
    {"ntracki", "integer", sizeof(int), FMOffset(particles_gtc_p, ntracki)},
    {"ptrackede", "float[nparam][ntracke]", sizeof(float), FMOffset(particles_gtc_p, ptrackede)},
    {"ptrackedi", "float[nparam][ntracki]", sizeof(float), FMOffset(particles_gtc_p, ptrackedi)},
    {NULL, NULL, 0, 0}
};


static FMFormatRec particles_gtc_format_list[] =
{
    {"particles", particles_gtc_field_list, sizeof(particles_gtc), NULL},
    {NULL, NULL, 0, NULL}
};
