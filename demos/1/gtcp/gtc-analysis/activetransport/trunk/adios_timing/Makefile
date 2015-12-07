#use this for ewok
SMOKY=y
JAGUAR=n

ifeq ($(JAGUAR),y)
    #use this for cray compute nodes
    CC=cc -c -g -target=linux

    ADIOS_LIB_HOME=$(HOME)/staging/adios.staging
    ADIOS_LIB_I=$(ADIOS_LIB_HOME)

    I_PATH=-I$(ADIOS_LIB_I) -I$(ADIOS_LIB_I)/src

    INSTALL_PREFIX=$(HOME)/work/pe.pgi
endif

ifeq ($(SMOKY),y)
    CC=mpicc -c -g
    ADIOS_LIB_HOME=$(HOME)/gts/adios.staging
    ADIOS_LIB_I=$(ADIOS_LIB_HOME)

    I_PATH=-I$(ADIOS_LIB_I) -I$(ADIOS_LIB_I)/src

    INSTALL_PREFIX=$(HOME)/work/smoky
endif

I_PATH=-I${HOME}/work/chama/include

all: libadios_read_timing.a libadios_sim_timing.a
#all: libadios_write_timing.a libadios_read_timing.a libadios_sim_timing.a

libadios_write_timing.a: timing_common.o timing_write.o
	rm -f libadios_write_timing.a
	ar crvs libadios_write_timing.a timing_common.o timing_write.o
	ranlib libadios_write_timing.a

libadios_read_timing.a: timing_common.o timing_read.o
	rm -f libadios_read_timing.a
	ar crvs libadios_read_timing.a timing_common.o timing_read.o
	ranlib libadios_read_timing.a

libadios_sim_timing.a: timing_common.o timing_simulation.o
	rm -f libadios_sim_timing.a
	ar crvs libadios_sim_timing.a timing_common.o timing_simulation.o
	ranlib libadios_sim_timing.a

.c.o :
	$(CC) $(I_PATH) $<

clean:
	rm -f *.o libadios_timing.a libadios_read_timing.a libadios_sim_timing.a

install:
	cp adios_write_timing.h adios_read_timing.h adios_sim_timing.h $(INSTALL_PREFIX)/include
	cp libadios_write_timing.a libadios_read_timing.a libadios_sim_timing.a $(INSTALL_PREFIX)/lib


