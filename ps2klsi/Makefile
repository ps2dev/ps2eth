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

LWIP = $(PS2DEV)/ps2ip/iop/lwip060rc1
LWIPINC = $(LWIP)/src/include
LWIPCORE = $(LWIP)/src/core
LWIPAPI = $(LWIP)/src/api


#  ------------------------------------------------------------------------
#  DIRECTORY PATHS & FLAGS

ELF2IRX = $(PS2LIB)/iop/utils/elf2irx/elf2irx

EECFLAGS  = -mips3 -EL -nostartfiles 
EEINCLUDES = -I. -I$(PS2LIB)/common/include -I$(PS2LIB)/ee/include
EECOMPILE = $(EECC) $(EEINCLUDES) $(EECFLAGS) 
EELINK = $(EELD)  

IOPCFLAGS = -G0
IOPINCLUDES = -I. -I../ps2ip/include -I$(LWIPINC) -I$(LWIPINC)/ipv4 -I$(PS2LIB)/common/include -I$(PS2LIB)/iop/include
IOPCOMPILE = $(IOPCC) $(IOPINCLUDES) $(IOPCFLAGS)
IOPLINK = $(IOPLD) --script mipsirx.x 

INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA =${INSTALL} -m 644
mkinstalldirs = $(SHELL) $(top_builddir)/mkinstalldirs


#  ------------------------------------------------------------------------
#  PROJECTS TO BUILD

all: ps2klsi.irx
	cp ps2klsi.irx $(PS2IP)/ps2klsi.irx
	cp ps2klsi.irx /cygdrive/c/ps2dev/ps2ip


clean:
	rm *.irx
	rm *.o


#  ------------------------------------------------------------------------
#  PS2KLSI.IRX BUILD INSTRUCTIONS

ps2klsi_LDFLAGS = -L$(PS2LIB)/iop/lib -L../lib
ps2klsi_LDADD = -liop_ps2debug -liop_ps2ip -lkernel
ps2klsi_OBJECTS = driver.o etharp.o ps2eth.o


ps2klsi.irx: $(ps2klsi_OBJECTS) 
	$(IOPLINK) $(ps2klsi_OBJECTS) $(ps2klsi_LDFLAGS) $(ps2klsi_LDADD) -o $@

driver.o: driver.c
	$(IOPCOMPILE) -o $@ -c $<

etharp.o: $(LWIP)/src/netif/etharp.c
	$(IOPCOMPILE) -o $@ -c $<

ps2eth.o: ps2eth.c
	$(IOPCOMPILE) -o $@ -c $<


