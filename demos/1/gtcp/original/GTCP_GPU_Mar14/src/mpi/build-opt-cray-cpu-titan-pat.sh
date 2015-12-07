#!/bin/bash

source $MODULESHOME/init/bash

module swap PrgEnv-pgi PrgEnv-cray
module load cudatoolkit perftools
module list

make ARCH=titan-opt-cpu-cray clean
make ARCH=titan-opt-cpu-cray

#pat_build -w -T shifti_toroidal -g mpi bench_gtc_titan_opt_cpu_cray 
pat_build -O apa ./bench_gtc_titan_opt_cpu_cray



