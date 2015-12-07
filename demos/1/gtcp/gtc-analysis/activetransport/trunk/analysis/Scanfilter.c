#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "gts_particle.h"
#include "Analyp.h"

// added by zf2: get rid of this
//extern gts_particles *pgtc;    // input particle data head pointer
// end of added by zf2

int Scanfilter(double *zion, int mi, double *_filter_results, long int *_count, double* _lower_limit, double* _upper_limit)
{
    int i;
    long int count = 0;
    for(i = 0; i < 7*mi; i = i + 7){
        if(((_lower_limit[0]<=zion[i])&&(zion[i]<=_upper_limit[0]))&&
        ((_lower_limit[1]<=zion[i+1])&&(zion[i+1]<=_upper_limit[1]))&
        ((_lower_limit[2]<=zion[i+2])&&(zion[i+2]<=_upper_limit[2]))){

#if PRINT_RESULT
            printf("\n%d Hit\n", i/7);
#endif
        if(count >= PG_NUM*7) {
            fprintf(stderr, "Need to resize the query result array.\n");
            double *temp = (double *) realloc(_filter_results, (size_t)(count*2*7*sizeof(double)));
            if(!temp) {
                fprintf(stderr, "cannot re-allocate memory. %s:%d\n", __FILE__, __LINE__);
                exit(-1);
            }
            _filter_results = temp;
        }

            _filter_results[count] = zion[i];
            _filter_results[count+1] = zion[i+1];
            _filter_results[count+2] = zion[i+2];
            _filter_results[count+3] = zion[i+3];
            _filter_results[count+4] = zion[i+4];
            _filter_results[count+5] = zion[i+5];
            _filter_results[count+6] = zion[i+6];
            count = count + 7;
        }
    }
	*(_count) = count/7;

#if PRINT_RESULT
fprintf(stderr, "im here mi %d count %d %s:%d\n", mi, count, __FILE__, __LINE__);
	
	printf("Scan %lf, %lf, %lf, %lf, %lf, %lf \n", _lower_limit[0], _upper_limit[0],
					     _lower_limit[1], _upper_limit[1],
					     _lower_limit[2], _upper_limit[2]);
#endif


	return 1;
}

int Scanfilter_combine(double *zion, int32_t mi, double *_filter_results, int64_t *_count, double* _lower_limit, double* _upper_limit)
{
    int i;
    long int count = (*_count)*7; // start from last location
    for(i = 0; i < 7*mi; i = i + 7){
//        if(((_lower_limit[0]<=zion[i])&&(zion[i]<=_upper_limit[0]))&&
//        ((_lower_limit[1]<=zion[i+1])&&(zion[i+1]<=_upper_limit[1]))&
//        ((_lower_limit[2]<=zion[i+2])&&(zion[i+2]<=_upper_limit[2]))){
        if((_lower_limit[0]<=zion[i+3])&&(zion[i+3]<=_upper_limit[0])) {

#if PRINT_RESULT
            printf("\n%d Hit\n", i/7);
#endif

            if(count >= PG_NUM*7) {
                fprintf(stderr, "Need to resize the query result array. %d %d \n", count, PG_NUM);
                double *temp = (double *) realloc(_filter_results, (size_t)(count*2*7*sizeof(double)));
                if(!temp) {
                    fprintf(stderr, "cannot re-allocate memory. %s:%d\n", __FILE__, __LINE__);
                    exit(-1);
                }
                _filter_results = temp;
            }

            _filter_results[count] = zion[i];
            _filter_results[count+1] = zion[i+1];
            _filter_results[count+2] = zion[i+2];
            _filter_results[count+3] = zion[i+3];
            _filter_results[count+4] = zion[i+4];
            _filter_results[count+5] = zion[i+5];
            _filter_results[count+6] = zion[i+6];
            count = count + 7;
        }
    }
    *(_count) = count/7;

#if PRINT_RESULT
fprintf(stderr, "im here mi %d count %d %s:%d\n", mi, count, __FILE__, __LINE__);

    printf("Scan %lf, %lf, %lf, %lf, %lf, %lf \n", _lower_limit[0], _upper_limit[0],
                         _lower_limit[1], _upper_limit[1],
                         _lower_limit[2], _upper_limit[2]);
#endif


    return 1;
}

