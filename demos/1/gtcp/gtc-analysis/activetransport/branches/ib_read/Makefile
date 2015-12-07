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

    I_PATH=-I$(ADIOS_LIB_I) -I$(ADIOS_LIB_I)/src -I$(HOME)/work/smoky/include

    INSTALL_PREFIX=$(HOME)/work/smoky
endif
OBJs=read_ib.o

all: libread_ib.a 

libread_ib.a: ${OBJs}
	rm -f libread_ib.a
	ar crvs libread_ib.a ${OBJs}
	ranlib libread_ib.a

.c.o :
	$(CC) $(I_PATH) $<

clean:
	rm -f *.o libread_ib.a

install:
	cp read_ib.h $(INSTALL_PREFIX)/include
	cp libread_ib.a $(INSTALL_PREFIX)/lib


