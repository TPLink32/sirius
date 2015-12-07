#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gts_particle.h"

static double
min_value(double *array, int length)
{
        double min;
        min = array[0];

        int i = 0;
        for(i = 0; i < length; i = i+7)
                if(array[i] < min) min = array[i];
        return min;
}

static double
max_value(double *array, int length)
{
        double max;
        max = array[0];

        int i = 0;
        for(i = 0; i < length; i = i+7)
                if(array[i] > max) max = array[i];
        return max;
}

/*
static double
avg_value(double *array, int length)
{
        double sum;
        double avg;
        sum = 0;

        int i = 0;
        for(i = 0; i < length; i = i+7)
                sum = sum+array[i];
        avg = (double)(sum/(length/7));
        return avg;
}
*/

/* To reduce scanning times, we combine the calculation of average and stdev togather. */

static void
avg_value(double *output, double *array, int length)
{
        double sum, sqr_sum;
        double avg, sqr_avg, stdev;
        sum = 0.0;
	sqr_sum = 0.0;
	stdev = 0.0;

        int i = 0;
        for(i = 0; i < length; i = i+7){
                sum = sum+array[i];
		sqr_sum = sqr_sum + (double)(array[i]*array[i]);
	}
        avg = (double)(sum/(length/7));
	sqr_avg = (double)(sqr_sum/(length/7));
	stdev = sqrt(sqr_avg - (avg * avg));
	output[0] = avg;
	output[1] = stdev;
}

int particle_init(gts_particles *pg){
	pg->istep = 0;		// timestep, start from 1
	pg->mype = 0;		// mpi rank of this gts process
	int numberpe;		// total number of gts processes
	pg->mi = 60;		// number of zion particles
	pg->mi_total = 0;	// total number of gts processes
	pg->mi_offset = 0;	// offset of zion array in global zion array
	pg->ntracer = 0.0;	// ignore, no use  double etracer_i0;
	pg->etracer_i0 = 0.0;	// ignore, no use
	pg->misum = 0.0;	// equal to mi_total, just in floating point
	double *zionp;
	zionp = (double *)malloc(sizeof(double)*7*pg->mi);
	if (zionp == NULL) {
		printf("Allocate gts zion array failed\n");
		return 0; // cannot allocate memory
	}
	/* Generate a bunch of random double for testing */
	int i;
	srand((unsigned)time(NULL));
	for(i = 0; i < 7*pg->mi; i++){
		//printf("Random double = %lf\n",((double)rand()/(double)RAND_MAX));
		zionp[i] = (double)(rand()/(double)RAND_MAX);
		if( (i%7) == 0) printf("\n");
		printf("%lf ", zionp[i]);
	}
	printf("\n\n");
	/* End Generating */
		
	pg->zion = zionp;	// particle array for zions of size 7 x mi
	pg->ntracer_e = 0.0;	// ignore, no use
	pg->etracer_e0 = 0.0;	// ignore, no use
	pg->mesum = 0.0;	// equal to me_total, just in floating point
	pg->me = 10;		// number of zeon particles
	pg->me_total = 0;	// total number of zeon particles
	pg->me_offset = 0;	// offset of zeon array in global zeon array	
	double *zeonp;
	zeonp = (double *)malloc(sizeof(double)*7*pg->me); 
	if (zeonp == NULL) {
		printf("Allocate gts zeon array failed\n");
		return 0; // cannot allocate memory
	}	
	/* Generate a bunch of random double for testing */
	srand((unsigned)time(NULL));
	for(i = 0; i < 7*pg->me; i++){
		//printf("Random double = %lf\n",((double)rand()/(double)RAND_MAX));
		zeonp[i] = (double)(rand()/(double)RAND_MAX);
	}
	/* End Generating */
	pg->zeon = zeonp;	// particle array for zeons of size 7 x me
	return 1;
}

int main()
{
	gts_particles *pgtc;	// input particle data head pointer
	double *pemi, *peme;	// the head pointer of ptrackede and ptrackedi
	int milen, melen;	// the length of zion and zeon array
	int rt, ru;		// return value 
	double min_x, min_y, min_z,
		max_x, max_y, max_z;	// minimum and maximum of x, y, z
	double	avg_stdev_x[2], avg_stdev_y[2], avg_stdev_z[2];	// average and standard deviation of x, y, z
	
	pgtc = (gts_particles *)malloc(sizeof(gts_particles));
	rt = particle_init(pgtc);
	if(rt == 0){
		printf("The particle structure initialization failed\n");
		exit(1);
	} 
	
	pemi = pgtc->zion;
	peme = pgtc->zeon;	

	milen = 7*(pgtc->mi);
	melen = 7*(pgtc->me);
	
	double *t_pemi, *t_peme;	
	t_pemi = pemi; // temp pointer for array search
	t_peme = peme; // temp pointer for array search

	min_x = *t_pemi;
	min_x = min_value(t_pemi, milen);
	printf("This min_x = %lf\n", min_x);
	min_y = *(t_pemi+1);
	min_y = min_value(t_pemi+1, milen);
	printf("This min_y = %lf\n", min_y);
	min_z = *(t_pemi+2);
	min_z = min_value(t_pemi+2, milen);
	printf("This min_z = %lf\n\n", min_z);

	max_x = *t_pemi;
	max_x = max_value(t_pemi, milen);
	printf("This max_x = %lf\n", max_x);
	max_y = *(t_pemi+1);
	max_y = max_value(t_pemi+1, milen);
	printf("This max_y = %lf\n", max_y);
	max_z = *(t_pemi+2);
	max_z = max_value(t_pemi+2, milen);
	printf("This max_z = %lf\n\n", max_z);

	avg_stdev_x[0] = 0.0;
	avg_stdev_x[1] = 0.0;
	avg_value(avg_stdev_x, t_pemi, milen);
	printf("This avg_x = %lf, stdev = %lf\n", avg_stdev_x[0], avg_stdev_x[1]);
	double x_lowerlimit = 0.0 , x_upperlimit = 0.0;
	x_lowerlimit = avg_stdev_x[0] - avg_stdev_x[1];
	x_upperlimit = avg_stdev_x[0] + avg_stdev_x[1];
	printf("%lf < x < %lf\n\n", x_lowerlimit, x_upperlimit);

	avg_stdev_y[0] = 0.0;
	avg_stdev_y[1] = 0.0;
	avg_value(avg_stdev_y, t_pemi+1, milen);
	printf("This avg_y = %lf, stdev = %lf\n", avg_stdev_y[0], avg_stdev_y[1]);
	double y_lowerlimit = 0.0 , y_upperlimit = 0.0;
	y_lowerlimit = avg_stdev_y[0] - avg_stdev_y[1];
	y_upperlimit = avg_stdev_y[0] + avg_stdev_y[1];
	printf("%lf < y < %lf\n\n", y_lowerlimit, y_upperlimit);

	avg_stdev_z[0] = 0.0;
	avg_stdev_z[1] = 0.0;
	avg_value(avg_stdev_z, t_pemi+2, milen);
	printf("This avg_z = %lf, stdev = %lf\n", avg_stdev_z[0], avg_stdev_z[1]);
	double z_lowerlimit = 0.0 , z_upperlimit = 0.0;
	z_lowerlimit = avg_stdev_z[0] - avg_stdev_z[1];
	z_upperlimit = avg_stdev_z[0] + avg_stdev_z[1];
	printf("%lf < z < %lf\n\n", z_lowerlimit, z_upperlimit);
/*	srand((unsigned)time(NULL));
	printf("Random double = %lf\n",((double)rand()/(double)RAND_MAX));
*/	printf("\nThis is testing, double size = %d\n\n", sizeof(double));
	return 1;

}
