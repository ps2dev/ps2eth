#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#
#  ------------------------------------------------------------------------

# Generated automatically from Makefile.in by configure.
#.SUFFIXES: .S .c .o .s .elf .irx

#  ------------------------------------------------------------------------
#  COMPILERS

IOPCC = iop-gcc
IOPAR = iop-ar
IOPLD = iop-ld
IOPAS = iop-as
EECC  = ee-gcc
EEAR  = ee-ar
EELD  = ee-gcc


#  ------------------------------------------------------------------------
#  LWIP PATHS
ROOT = /home/Administrator
PS2LIB = $(ROOT)/ps2lib

LWIP = $(ROOT)/ps2ip/iop/lwip060rc1
#LWIP = $(ROOT)/ps2ip/iop/lwip_cvs
LWIPINC = $(LWIP)/src/include
LWIPCORE = $(LWIP)/src/core
LWIPAPI = $(LWIP)/src/api


#  ------------------------------------------------------------------------
#  DIRECTORY PATHS & FLAGS

EECFLAGS  = -mips3 -EL -nostartfiles -O2
EEINCLUDES = -I. -I$(PS2LIB)/common/include -I$(PS2LIB)/ee/include
EECOMPILE = $(EECC) $(EEINCLUDES) $(EECFLAGS) 
EELINK = $(EELD)  

IOPCFLAGS = -G0 -nostdlib -fno-builtin  -fomit-frame-pointer -Wall -EL -mcpu=r3000 -O2
IOPINCLUDES = -I. -I../ps2ip/include -I$(LWIPINC) -I$(LWIPINC)/ipv4 -I$(PS2LIB)/common/include -I$(PS2LIB)/iop/include
IOPCOMPILE = $(IOPCC) $(IOPINCLUDES) $(IOPCFLAGS)
IOPLINK = $(IOPLD) --script mipsirx.x 

INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA =${INSTALL} -m 644

#  ------------------------------------------------------------------------
#  PROJECTS TO BUILD

all: ps2smap.irx
	cp ps2smap.irx $(PS2LIB)/ee/samples/iptest/ps2smap.irx


clean:
	rm *.irx
	rm *.o


#  ------------------------------------------------------------------------
#  PS2SMAP.IRX BUILD INSTRUCTIONS

ps2smap_LDFLAGS = -L$(PS2LIB)/iop/lib -L../lib
ps2smap_LDADD = -liop_ps2debug -liop_ps2ip -lkernel -liop_sysclib -liop_thsemap -liop_usbd -liop_thbase -liop_intrman -liop_thevent -liop_loadcore
ps2smap_OBJECTS = smap.o etharp.o smapif.o main.o


ps2smap.irx: $(ps2smap_OBJECTS) 
	$(IOPLINK) $(ps2smap_OBJECTS) $(ps2smap_LDFLAGS) $(ps2smap_LDADD) -o $@

etharp.o: $(LWIP)/src/netif/etharp.c
	$(IOPCOMPILE) -o $@ -c $<

smapif.o: smapif.c
	$(IOPCOMPILE) -o $@ -c $<

smap.o: smap.c
	$(IOPCOMPILE) -o $@ -c $<

main.o: main.c
	$(IOPCOMPILE) -o $@ -c $<
