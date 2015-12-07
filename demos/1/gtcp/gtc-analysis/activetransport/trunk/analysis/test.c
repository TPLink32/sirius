#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

/*
 * get local timestamp
 */
double get_timestamp()
{
    struct timeval timestamp;
    gettimeofday(&timestamp, NULL);

    double realtime = timestamp.tv_sec + timestamp.tv_usec / 1.0e6;
    return realtime;
}

int main (int argc, char *argv[])
{
//     double *zion = malloc(30*1024*104*sizeof(double));
     char *zion = (char *)malloc(30*1024*1024);
     if(!zion) {
         fprintf(stderr, "err\n");
         return -1;
     }

     double start = get_timestamp();
     int i;
     for(i = 0; i < 30*1024*10; i ++) {
          zion[i] ++;
     }         
     double end = get_timestamp();

     printf("time = %f\n", end - start);

     return 0;
}
