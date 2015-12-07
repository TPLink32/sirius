#!/bin/bash

source $MODULESHOME/init/bash

module swap PrgEnv-pgi PrgEnv-cray
module load cudatoolkit
module list

make ARCH=titan-opt-cray clean
make ARCH=titan-opt-cray

