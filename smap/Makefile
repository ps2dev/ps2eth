#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#
#  ------------------------------------------------------------------------

include ../Makefile.pref
include ../Makefile.global


#  ------------------------------------------------------------------------
#  PROJECTS TO BUILD

all: ps2smap.irx
	cp ps2smap.irx ../bin


clean:
	rm -f *.irx *.o


#  ------------------------------------------------------------------------
#  PS2SMAP.IRX BUILD INSTRUCTIONS

ps2smap_LDFLAGS = -L$(PS2LIB)/iop/lib -L$(PS2IP)/iop/lib
ps2smap_LDADD = -liop_ps2debug -liop_ps2ip -lkernel 
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
