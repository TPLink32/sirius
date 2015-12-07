#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "mpi.h"
#include "Analyp.h"

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

/* The function calculate sum(XorYorZ)2 and (sum(X)orsum(Y)orsum(Z))2  */

static void
sum_value(double *output, double *array, int length)
{
        double sqr_sum, sum_sqr;
        sqr_sum = 0.0;
	sum_sqr = 0.0;

        int i = 0;
        for(i = 0; i < length; i = i+7){
                sum_sqr = sum_sqr+array[i];
		sqr_sum = sqr_sum + (double)(array[i]*array[i]);
	}
	output[0] = sqr_sum;
	output[1] = sum_sqr;
}

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
	pg->mi = PG_NUM;	// number of zion particles
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
	pg->me = PG_NUM;		// number of zeon particles
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

gts_particles *pgtc;    // input particle data head pointer

//int Analyp(double *zion, int mi, double *zeon, int me, double *_filter_results, int *_count, gts_local *_pgl)
int Analyp(double *zion, int32_t mi, gts_local *_pgl)
{
	gts_local *pgl;		// particle features structure
	double *pemi, *peme;	// the head pointer of ptrackede and ptrackedi
	int milen, melen;	// the length of zion and zeon array
	int rt, ru;		// return value 
	double min_x, min_y, min_z,
		max_x, max_y, max_z;	// minimum and maximum of x, y, z
	double	sum_x[2], sum_y[2], sum_z[2];	// squr of sum and sum of squr ion of x, y, z
	double	avg_stdev_x[2], avg_stdev_y[2], avg_stdev_z[2];	// average and standard deviation of x, y, z

	
// added by zf2
#if 0 // generate synthetic data

	pgtc = (gts_particles *)malloc(sizeof(gts_particles));
	pgl = (gts_local *)malloc(sizeof(gts_local));
	rt = particle_init(pgtc);
	if(rt == 0){
		printf("The particle structure initialization failed\n");
		exit(1);
	} 
	
	pemi = pgtc->zion;
	peme = pgtc->zeon;	

	_pgl->mi_cnt = pgtc->mi;	// the length of zion array
	_pgl->me_cnt = pgtc->me;	// the length of zeon array

#else

        pemi = zion;
//        peme = zeon;

        _pgl->mi_cnt = mi;
//        _pgl->me_cnt = me;        // the length of zeon array

        milen = 7*(mi);
//        melen = 7*(me);
#endif


#if 0 // multiple passes of scans
	
	double *t_pemi, *t_peme;	
	t_pemi = pemi;		// temp pointer for array search
	t_peme = peme;		// temp pointer for array search

	/* min max of mi's x y z  */
	min_x = *t_pemi;
	min_x = min_value(t_pemi, milen);
	_pgl->mi_min_x = min_x;
//	printf("This min_x = %lf\n", min_x);
	min_y = *(t_pemi+1);
	min_y = min_value(t_pemi+1, milen);
	_pgl->mi_min_y = min_y;
//	printf("This min_y = %lf\n", min_y);
	min_z = *(t_pemi+2);
	min_z = min_value(t_pemi+2, milen);
	_pgl->mi_min_z = min_z;
//	printf("This min_z = %lf\n\n", min_z);

	max_x = *t_pemi;
	max_x = max_value(t_pemi, milen);
	_pgl->mi_max_x = max_x;
//	printf("This max_x = %lf\n", max_x);
	max_y = *(t_pemi+1);
	max_y = max_value(t_pemi+1, milen);
	_pgl->mi_max_y = max_y;
//	printf("This max_y = %lf\n", max_y);
	max_z = *(t_pemi+2);
	max_z = max_value(t_pemi+2, milen);
	_pgl->mi_max_z = max_z;
//	printf("This max_z = %lf\n\n", max_z);

	sum_x[0] = 0.0;
	sum_x[1] = 0.0;

	sum_value(sum_x, t_pemi, milen);
	_pgl->mi_sqr_sum_x = sum_x[0];
	_pgl->mi_sum_x = sum_x[1];

	sum_y[0] = 0.0;
	sum_y[1] = 0.0;

	sum_value(sum_y, t_pemi+1, milen);
	_pgl->mi_sqr_sum_y = sum_y[0];
	_pgl->mi_sum_y = sum_y[1];

	sum_z[0] = 0.0;
	sum_z[1] = 0.0;

	sum_value(sum_z, t_pemi+2, milen);
	_pgl->mi_sqr_sum_z = sum_z[0];
	_pgl->mi_sum_z = sum_z[1];

/* The end */
#else 
        // one pass to calculate everything
        _pgl->mi_min_x = zion[0];
        _pgl->mi_max_x = zion[0];
        _pgl->mi_sqr_sum_x = zion[0]*zion[0];
        _pgl->mi_sum_x = zion[0];
        _pgl->mi_min_y = zion[1];
        _pgl->mi_max_y = zion[1];
        _pgl->mi_sqr_sum_y = zion[1]*zion[1];
        _pgl->mi_sum_y = zion[1];
        _pgl->mi_min_z = zion[2];
        _pgl->mi_max_z = zion[2];
        _pgl->mi_sqr_sum_z = zion[2]*zion[2];
        _pgl->mi_sum_z = zion[2];
        _pgl->mi_min_v1 = zion[3];
        _pgl->mi_max_v1 = zion[3];
        _pgl->mi_sqr_sum_v1 = zion[3]*zion[3];
        _pgl->mi_sum_v1 = zion[3];
        _pgl->mi_min_w = zion[4];
        _pgl->mi_max_w = zion[4];
        _pgl->mi_sqr_sum_w = zion[4]*zion[4];
        _pgl->mi_sum_w = zion[4];
        _pgl->mi_min_v2 = zion[5];
        _pgl->mi_max_v2 = zion[5];
        _pgl->mi_sqr_sum_v2 = zion[5]*zion[5];
        _pgl->mi_sum_v2 = zion[5];

        int i;
        for(i = 7; i < milen; i = i + 7){
            double x = zion[i];
            double y = zion[i+1];
            double z = zion[i+2];
            double v1 = zion[i+3];
            double w = zion[i+4];
            double v2 = zion[i+5];

            // min/max/sum/square_sum of x
            if(x < _pgl->mi_min_x) { 
                _pgl->mi_min_x = x;
            }
            else if(x > _pgl->mi_max_x) {    
                _pgl->mi_max_x = x;
            }  
            _pgl->mi_sqr_sum_x += x*x;
            _pgl->mi_sum_x = x;

            // min/max/sum/square_sum of y 
            if(y < _pgl->mi_min_y) { // min
                _pgl->mi_min_y = y;
            }
            else if(y > _pgl->mi_max_y) {
                _pgl->mi_max_y = y;
            }
            _pgl->mi_sqr_sum_y += y*y;
            _pgl->mi_sum_y += y;

            // min/max/sum/square_sum of z
            if(z < _pgl->mi_min_z) { // min
                _pgl->mi_min_z = z;
            }
            else if(z > _pgl->mi_max_z) {
                _pgl->mi_max_z = z;
            }
            _pgl->mi_sqr_sum_z += z*z;
            _pgl->mi_sum_z += z;

            // min/max/sum/square_sum of v1
            if(v1 < _pgl->mi_min_v1) { // min
                _pgl->mi_min_v1 = v1;
            }
            else if(v1 > _pgl->mi_max_v1) {
                _pgl->mi_max_v1 = v1;
            }
            _pgl->mi_sqr_sum_v1 += v1*v1;
            _pgl->mi_sum_v1 += v1;

            // min/max/sum/square_sum of w
            if(w < _pgl->mi_min_w) { // min
                _pgl->mi_min_w = w;
            }
            else if(w > _pgl->mi_max_w) {
                _pgl->mi_max_w = w;
            }
            _pgl->mi_sqr_sum_w += w*w;
            _pgl->mi_sum_w += w;

            // min/max/sum/square_sum of v2
            if(v2 < _pgl->mi_min_v2) { // min
                _pgl->mi_min_v2 = v2;
            }
            else if(v2 > _pgl->mi_max_v2) {
                _pgl->mi_max_v2 = v2;
            }
            _pgl->mi_sqr_sum_v2 += v2*v2;
            _pgl->mi_sum_v2 += v2;
        } 


#endif

#if 0 // do not filter data using local bounds

	avg_stdev_x[0] = 0.0;
	avg_stdev_x[1] = 0.0;
	avg_value(avg_stdev_x, t_pemi, milen);


//	printf("This avg_x = %lf, stdev = %lf\n", avg_stdev_x[0], avg_stdev_x[1]);
	double x_lowerlimit = 0.0 , x_upperlimit = 0.0;
	x_lowerlimit = avg_stdev_x[0] - avg_stdev_x[1];
	x_upperlimit = avg_stdev_x[0] + avg_stdev_x[1];
//	printf("%lf < x < %lf\n\n", x_lowerlimit, x_upperlimit);

	avg_stdev_y[0] = 0.0;
	avg_stdev_y[1] = 0.0;
	avg_value(avg_stdev_y, t_pemi+1, milen);
//	printf("This avg_y = %lf, stdev = %lf\n", avg_stdev_y[0], avg_stdev_y[1]);
	double y_lowerlimit = 0.0 , y_upperlimit = 0.0;
	y_lowerlimit = avg_stdev_y[0] - avg_stdev_y[1];
	y_upperlimit = avg_stdev_y[0] + avg_stdev_y[1];
//	printf("%lf < y < %lf\n\n", y_lowerlimit, y_upperlimit);

	avg_stdev_z[0] = 0.0;
	avg_stdev_z[1] = 0.0;
	avg_value(avg_stdev_z, t_pemi+2, milen);
//	printf("This avg_z = %lf, stdev = %lf\n", avg_stdev_z[0], avg_stdev_z[1]);
	double z_lowerlimit = 0.0 , z_upperlimit = 0.0;
	z_lowerlimit = avg_stdev_z[0] - avg_stdev_z[1];
	z_upperlimit = avg_stdev_z[0] + avg_stdev_z[1];
//	printf("%lf < z < %lf\n\n", z_lowerlimit, z_upperlimit);

	int i = 0, count = 0;
	for(i = 0; i < milen; i = i + 7){
		if(((x_lowerlimit<=t_pemi[i])&&(t_pemi[i]<=x_upperlimit))&& 
		((y_lowerlimit<=t_pemi[i+1])&&(t_pemi[i+1]<=y_upperlimit))& 
		((z_lowerlimit<=t_pemi[i+2])&&(t_pemi[i+2]<=z_upperlimit))){
#if PRINT_RESULT
			printf("\n%d Hit\n", i/7);	
#endif
			_filter_results[count] = t_pemi[i];
			_filter_results[count+1] = t_pemi[i+1];
			_filter_results[count+2] = t_pemi[i+2];
			_filter_results[count+3] = t_pemi[i+3];
			_filter_results[count+4] = t_pemi[i+4];
			_filter_results[count+5] = t_pemi[i+5];
			_filter_results[count+6] = t_pemi[i+6];
			count = count + 7;
		}
	}
	
	*(_count) = count/7;	

#if PRINT_RESULT
	printf("\ncount = %d\n", count);	
//	system("hostname");
//	printf("      ***********\n");
//	for(i = 0; i < count; i++){
//		if(i%7 == 0) printf("\n");
//		printf("%lf ", _filter_results[i]);
//	}	
#endif
#endif

	return 1;
}

int Analyp_combine(double *zion, int32_t mi, gts_local *_pgl, int32_t first_pg)
{
    gts_local *pgl;         // particle features structure
    double *pemi, *peme;    // the head pointer of ptrackede and ptrackedi
    int milen, melen;       // the length of zion and zeon array
    int rt, ru;             // return value
    double min_x, min_y, min_z,
           max_x, max_y, max_z;    // minimum and maximum of x, y, z
    double  sum_x[2], sum_y[2], sum_z[2];   // squr of sum and sum of squr ion of x, y, z
    double  avg_stdev_x[2], avg_stdev_y[2], avg_stdev_z[2]; // average and standard deviation of x, y, z

assert (_pgl);
assert (zion);
    pemi = zion;


    milen = 7*(mi);

    // one pass to calculate everything
    int start_i = 0;
    if(first_pg) {
        _pgl->mi_cnt = mi;
        _pgl->mi_min_x = zion[0];
        _pgl->mi_max_x = zion[0];
        _pgl->mi_sqr_sum_x = zion[0]*zion[0];
        _pgl->mi_sum_x = zion[0];
        _pgl->mi_min_y = zion[1];
        _pgl->mi_max_y = zion[1];
        _pgl->mi_sqr_sum_y = zion[1]*zion[1];
        _pgl->mi_sum_y = zion[1];
        _pgl->mi_min_z = zion[2];
        _pgl->mi_max_z = zion[2];
        _pgl->mi_sqr_sum_z = zion[2]*zion[2];
        _pgl->mi_sum_z = zion[2];
        _pgl->mi_min_v1 = zion[3];
        _pgl->mi_max_v1 = zion[3];
        _pgl->mi_sqr_sum_v1 = zion[3]*zion[3];
        _pgl->mi_sum_v1 = zion[3];
        _pgl->mi_min_w = zion[4];
        _pgl->mi_max_w = zion[4];
        _pgl->mi_sqr_sum_w = zion[4]*zion[4];
        _pgl->mi_sum_w = zion[4];
        _pgl->mi_min_v2 = zion[5];
        _pgl->mi_max_v2 = zion[5];
        _pgl->mi_sqr_sum_v2 = zion[5]*zion[5];
        _pgl->mi_sum_v2 = zion[5];
        start_i = 7;
    }
    else {
        _pgl->mi_cnt += mi;
    }
    
    int i;
    for(i = start_i; i < milen; i = i + 7){
        double x = zion[i];
        double y = zion[i+1];
        double z = zion[i+2];
        double v1 = zion[i+3];
        double w = zion[i+4];
        double v2 = zion[i+5];

        // min/max/sum/square_sum of x
        if(x < _pgl->mi_min_x) {
            _pgl->mi_min_x = x;
        }
        else if(x > _pgl->mi_max_x) {
            _pgl->mi_max_x = x;
        }
        _pgl->mi_sqr_sum_x += x*x;
        _pgl->mi_sum_x = x;

        // min/max/sum/square_sum of y
        if(y < _pgl->mi_min_y) { // min
            _pgl->mi_min_y = y;
        }
        else if(y > _pgl->mi_max_y) {
            _pgl->mi_max_y = y;
        }
        _pgl->mi_sqr_sum_y += y*y;
        _pgl->mi_sum_y += y;

        // min/max/sum/square_sum of z
        if(z < _pgl->mi_min_z) { // min
            _pgl->mi_min_z = z;
        }
        else if(z > _pgl->mi_max_z) {
            _pgl->mi_max_z = z;
        }
        _pgl->mi_sqr_sum_z += z*z;
        _pgl->mi_sum_z += z;

        // min/max/sum/square_sum of v1
        if(v1 < _pgl->mi_min_v1) { // min
            _pgl->mi_min_v1 = v1;
        }
        else if(v1 > _pgl->mi_max_v1) {
            _pgl->mi_max_v1 = v1;
        }
        _pgl->mi_sqr_sum_v1 += v1*v1;
        _pgl->mi_sum_v1 += v1;

        // min/max/sum/square_sum of w
        if(w < _pgl->mi_min_w) { // min
            _pgl->mi_min_w = w;
        }
        else if(w > _pgl->mi_max_w) {
            _pgl->mi_max_w = w;
        }
        _pgl->mi_sqr_sum_w += w*w;
        _pgl->mi_sum_w += w;

        // min/max/sum/square_sum of v2
        if(v2 < _pgl->mi_min_v2) { // min
            _pgl->mi_min_v2 = v2;
        }
        else if(v2 > _pgl->mi_max_v2) {
            _pgl->mi_max_v2 = v2;
        }
        _pgl->mi_sqr_sum_v2 += v2*v2;
        _pgl->mi_sum_v2 += v2;
    }

    return 1;
}

