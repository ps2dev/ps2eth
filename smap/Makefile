#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#
#  ------------------------------------------------------------------------

TOPDIR = ..

IOP_BIN = ../bin/ps2smap.irx
IOP_OBJS = smap.o smapif.o main.o

IOP_LDFLAGS += -L$(PS2IP)/iop/lib
IOP_LIBS += -liop_ps2ip -llwip_common


all: $(IOP_BIN)

clean:
	-rm -f *.o

include $(TOPDIR)/Makefile.pref
include $(TOPDIR)/Makefile.global

include $(PS2LIB)/Makefile.pref
include $(PS2LIB)/Makefile.iopglobal
