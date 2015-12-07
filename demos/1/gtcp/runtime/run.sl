#!/bin/bash

#SBATCH --account=FY140262
#SBATCH --job-name=gtc-p
#SBATCH --nodes=1
#SBATCH --time=0:10:00

echo starting bench_gtc

WORK_DIR=/gscratch2/gflofst

cd ${WORK_DIR}

#lfs setstripe . -i -1 -c 96 -s 1048576
#export LOG_DIR=1k.base
#export FILE_NAME=1k.small.base
#mkdir ${WORK_DIR}/${LOG_DIR}

#mpirun -np 2 valgrind --leak-check=full ./bench_gtc_sith_gcc_debug ./input/A.txt 1 2
mpirun -np 2 ./bench_gtc_sith_gcc_debug ./input/A.txt 1 2
