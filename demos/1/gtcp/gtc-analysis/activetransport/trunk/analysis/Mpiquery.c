#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "Analyp.h"

#if 0
int main(int argc, char **argv) {

        int rc;
	rc = MPI_Init(&argc,&argv);
	if (rc != MPI_SUCCESS) {
		printf ("Error starting MPI program. Terminating.\n");
		MPI_Abort(MPI_COMM_WORLD, rc);
	}

        /*******  do some work *******/
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

        MPI_Comm_size(MPI_COMM_WORLD, &numberpe); 
        MPI_Comm_rank(MPI_COMM_WORLD, &mype); 

        mi = 10000;
        mi_total = 320000;
        ntracer = 1.0;
        etracer_i0 = 2.0;
        misum = 320000.0;

        for(istep = 1; istep < 10; istep ++) {
          rc = global_range_query_(&istep,          // timestep, start from 1
                                 &mype,           // mpi rank of this gts process
                                 &numberpe,       // total number of gts processes
                                 &mi,             // number of zion particles
                                 &mi_total,  // total number of zion particles
                                 &mi_offset, // offset of zion array in global zion array
                                 &ntracer,     // ignore, no use
                                 &etracer_i0,  // ignore, no use
                                 &misum,       // equal to mi_total, just in floating point
                                 zion,        // particle array for zions of size 7 x mi
                                 &ntracer_e,   // ignore, no use
                                 &etracer_e0,  // ignore, no use
                                 &mesum,       // equal to me_total, just in floating point
                                 &me,             // number of zeon particles
                                 &me_total,  // total number of zeon particles
                                 &me_offset, // offset of zeon array in global zeon array
                                 zeon 
                                );

            if(mype==0) printf("\n=============== %d ==============\n", istep);
  
        } // end of for

        MPI_Finalize();
        return 1;
}
#endif 

double *qu_mi = NULL;

void global_test_(int *istep, 
                  int *mype,
                  int *numberpe,
                  int *mi,
                  long int *mi_total,
                  long int *mi_offset,
                  double *ntracer,
                  double *etracer_i0,
                  double *misum,
                  double *zion,
                  double *ntracer_e,
                  double *etracer_e0,
                  double *mesum,
                  int *me,
                  long int *me_total,
                  long int *me_offset,
                  double *zeon    
                 ) 
{
#if PRINT_RESULT
fprintf(stderr, "\ndistribution istep\t%d\trank\t%d\tmi\t%d\tmitotal\t%ld %s:%d\n", *istep, *mype, *mi,*mi_total,__FILE__, __LINE__);
#endif
        int numtasks, rank, rc, num_mi, rt;
	int i;

        if(!qu_mi) {
            qu_mi = (double *) malloc(sizeof(double)*PG_NUM*7);
            if(!qu_mi) {
                fprintf(stderr, "cannot allocate memory. %s:%d\n", __FILE__,__LINE__);
                return;
            }
        }
        //double qu_mi[7];
        gts_local  *gts_local_features;
        gts_global *gts_global_features;

// added by zf2
//        MPI_Comm comm;
        MPI_Comm comm = MPI_COMM_WORLD;
// end of added by zf2

        double *rbuf;
        int *counts, *displs;
        int sum = 0;

	gts_local_features = (gts_local *)malloc(sizeof(gts_local));
	gts_global_features = (gts_global *)malloc(sizeof(gts_global));

	rt = Analyp(zion, *mi,zeon, *me,  qu_mi, &num_mi, gts_local_features);

	/* reduce the features of X and distribute to all nodes */
	MPI_Allreduce(&(gts_local_features->mi_cnt), &(gts_global_features->mi_gcnt), 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_min_x), &(gts_global_features->mi_gmin_x), 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_max_x), &(gts_global_features->mi_gmax_x), 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_sqr_sum_x), &(gts_global_features->mi_sqr_sum_x), 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_sum_x), &(gts_global_features->mi_sum_sqr_x), 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);


#if PRINT_RESULT
	printf("\nHong Fang X : %d , %d , %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
		gts_local_features->mi_cnt, gts_global_features->mi_gcnt,
		gts_local_features->mi_min_x, gts_global_features->mi_gmin_x,
		gts_local_features->mi_max_x, gts_global_features->mi_gmax_x,
		gts_local_features->mi_sqr_sum_x, gts_global_features->mi_sqr_sum_x,
		gts_local_features->mi_sum_x, gts_global_features->mi_sum_sqr_x
		);
#endif

	/* reduce the features of Y and distribute to all nodes */
	MPI_Allreduce(&(gts_local_features->mi_min_y), &(gts_global_features->mi_gmin_y), 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_max_y), &(gts_global_features->mi_gmax_y), 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_sqr_sum_y), &(gts_global_features->mi_sqr_sum_y), 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_sum_y), &(gts_global_features->mi_sum_sqr_y), 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

#if PRINT_RESULT

	printf("\nHong Fang Y : %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
		gts_local_features->mi_min_y, gts_global_features->mi_gmin_y,
		gts_local_features->mi_max_y, gts_global_features->mi_gmax_y,
		gts_local_features->mi_sqr_sum_y, gts_global_features->mi_sqr_sum_y,
		gts_local_features->mi_sum_y, gts_global_features->mi_sum_sqr_y
		);
#endif

	/* reduce the features of Z and distribute to all nodes */
	MPI_Allreduce(&(gts_local_features->mi_min_z), &(gts_global_features->mi_gmin_z), 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_max_z), &(gts_global_features->mi_gmax_z), 1, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_sqr_sum_z), &(gts_global_features->mi_sqr_sum_z), 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
	MPI_Allreduce(&(gts_local_features->mi_sum_z), &(gts_global_features->mi_sum_sqr_z), 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

#if PRINT_RESULT

	printf("\nHong Fang Z : %lf, %lf, %lf, %lf, %lf, %lf, %lf, %lf  \n",
		gts_local_features->mi_min_z, gts_global_features->mi_gmin_z,
		gts_local_features->mi_max_z, gts_global_features->mi_gmax_z,
		gts_local_features->mi_sqr_sum_z, gts_global_features->mi_sqr_sum_z,
		gts_local_features->mi_sum_z, gts_global_features->mi_sum_sqr_z
		);
#endif
	/* Averages for X, Y, Z*/
	double G_avg_x, G_avg_y, G_avg_z;
	G_avg_x = gts_global_features->mi_sum_sqr_x / gts_global_features->mi_gcnt;
	G_avg_y = gts_global_features->mi_sum_sqr_y / gts_global_features->mi_gcnt;
	G_avg_z = gts_global_features->mi_sum_sqr_z / gts_global_features->mi_gcnt;

#if PRINT_RESULT

	printf("\n avg value  %lf , %lf , %lf | %lf , %lf , %lf \n", gts_global_features->mi_sum_sqr_x, gts_global_features->mi_sum_sqr_y, gts_global_features->mi_sum_sqr_z, G_avg_x, G_avg_y, G_avg_z);
#endif

	/* Standard deviation for X, Y, Z */	
	double G_stdev_x, G_stdev_y, G_stdev_z;
	G_stdev_x = sqrt((gts_global_features->mi_sqr_sum_x/gts_global_features->mi_gcnt)-(G_avg_x*G_avg_x));	
	G_stdev_y = sqrt((gts_global_features->mi_sqr_sum_y/gts_global_features->mi_gcnt)-(G_avg_y*G_avg_y));	
	G_stdev_z = sqrt((gts_global_features->mi_sqr_sum_z/gts_global_features->mi_gcnt)-(G_avg_z*G_avg_z));	
	
	/* Upper and lower limit for X, Y, Z */
	double Uplimit[3], Lolimit[3];
	Lolimit[0] = G_avg_x - G_stdev_x;
	Uplimit[0] = G_avg_x + G_stdev_x; 
	Lolimit[1] = G_avg_y - G_stdev_y;
	Uplimit[1] = G_avg_y + G_stdev_y;
	Lolimit[2] = G_avg_z - G_stdev_z;	
	Uplimit[2] = G_avg_z + G_stdev_z;

#if PRINT_RESULT

	printf("\nstdev value  %lf , %lf, %lf \n", G_stdev_x, G_stdev_y, G_stdev_z);
#endif

	Scanfilter(zion, *mi, zeon, *me, qu_mi, &num_mi, Lolimit, Uplimit);

#if PRINT_RESULT

fprintf(stderr, "\ndistribution istep\t%d\trank\t%d\tmi\t%d\tfilter\t%d\t %s:%d\n", *istep, *mype, *mi,num_mi,__FILE__, __LINE__);

	for(i = 0; i < num_mi*7; i++){
		if((i % 7) == 0) printf("\n");
		printf("%lf ", qu_mi[i]);
	}
	printf("\n");
#endif

	MPI_Comm_size(MPI_COMM_WORLD,&numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);

#if PRINT_RESULT

	printf ("Number of tasks= %d My rank= %d\n", numtasks,rank);
	printf("--------------------------------------\n");
#endif

	if (rank == 0){
//		system("hostname");
		counts = (int *)malloc(numtasks*sizeof(int));
		displs = (int *)malloc(numtasks*sizeof(int));
        	if ((counts == NULL)||(displs == NULL)) {
                	printf("Allocate counts or displs array failed\n");
	        }
	}
	MPI_Gather(&num_mi, 1, MPI_INT, counts, 1, MPI_INT, 0, MPI_COMM_WORLD);

	if(rank == 0){
#if PRINT_RESULT
		printf("\n");
#endif
		for(i = 0; i < numtasks; i++){
			counts[i] = counts[i]*7;
#if PRINT_RESULT
			printf("%d ", counts[i]);
#endif
		}
#if PRINT_RESULT
		printf("\n");
#endif
	}

	if(rank == 0){

                displs[0]=0;
                for( i=1; i < numtasks; i++){
                        displs[i]=counts[i-1]+displs[i-1];
                }

		for(i = 0 ; i < numtasks; i++) sum = sum + counts[i];
		//printf("sum %d", sum);
		rbuf = (double *)malloc(sum*sizeof(double));
        	if (rbuf == NULL) {
                	printf("Allocate particle collecting array failed\n");
	        }
	}

	MPI_Gatherv(qu_mi, num_mi*7, MPI_DOUBLE,
		    rbuf, counts, displs, MPI_DOUBLE, 
		    0, MPI_COMM_WORLD);

#if PRINT_RESULT
	if(rank == 0){
		for(i = 0; i < sum; i++){
			if((i % 7) == 0) printf("\n");
			printf("%lf ", rbuf[i]);
		}
		printf("\n");
	}
#endif
}
