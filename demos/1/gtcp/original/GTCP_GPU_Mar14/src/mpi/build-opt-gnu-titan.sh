#!/bin/bash

source $MODULESHOME/init/bash

module swap PrgEnv-pgi PrgEnv-gnu
module load cudatoolkit
module list

make ARCH=titan-opt-gnu clean
make ARCH=titan-opt-gnu

