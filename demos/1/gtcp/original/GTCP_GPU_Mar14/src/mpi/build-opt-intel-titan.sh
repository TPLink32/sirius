#!/bin/bash

source $MODULESHOME/init/bash

module swap PrgEnv-pgi PrgEnv-intel
module load cudatoolkit
module list

make ARCH=titan-opt-intel clean
make ARCH=titan-opt-intel

