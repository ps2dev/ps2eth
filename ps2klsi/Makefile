#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#
#  ------------------------------------------------------------------------

TOPDIR = ..

IOP_BIN = ../bin/ps2klsi.irx
IOP_OBJS = driver.o ps2eth.o

IOP_LDFLAGS += -L$(PS2IP)/iop/lib
IOP_LIBS += -liop_ps2ip -llwip_common


all: $(IOP_BIN)

clean:
	-rm -f *.o

include $(TOPDIR)/Makefile.pref
include $(TOPDIR)/Makefile.global

include $(PS2LIB)/Makefile.pref
include $(PS2LIB)/Makefile.iopglobal
