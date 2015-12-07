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
OBJs=shm_mm.o shm_malloc.o shm_default_malloc.o shm_queue.o meta_data.o shm_coordinator.o shm_spinlock.o read_shm.o thread_pin.o

all: libshm_transport.a 

libshm_transport.a: ${OBJs}
	rm -f libshm_transport.a
	ar crvs libshm_transport.a ${OBJs}
	ranlib libshm_transport.a

.c.o :
	$(CC) $(I_PATH) $<

clean:
	rm -f *.o libshm_transport.a

install:
	cp meta_data.h read_shm.h shm_coordinator.h shm_mm.h shm_malloc.h shm_queue.h shm_config.h thread_pin.h $(INSTALL_PREFIX)/include
	cp libshm_transport.a $(INSTALL_PREFIX)/lib


