#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#
#  ------------------------------------------------------------------------

include ../Makefile.pref
include ../Makefile.global


#  ------------------------------------------------------------------------
#  PROJECTS TO BUILD

all: ps2klsi.irx
	cp ps2klsi.irx ../bin


clean:
	rm -f *.irx *.o


#  ------------------------------------------------------------------------
#  PS2KLSI.IRX BUILD INSTRUCTIONS

ps2klsi_LDFLAGS = -L$(PS2LIB)/iop/lib -L$(PS2IP)/iop/lib
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


