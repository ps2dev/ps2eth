#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#                           (C)2003, adresd     ( adresd_ps2dev@yahoo.com )
#  ------------------------------------------------------------------------

TOPDIR = ..

IOP_BIN = ../bin/ps2smap.irx
IOP_OBJS = smap.o smapif.o main.o iop_ps2dev9.s

IOP_LDFLAGS += -L$(PS2IP)/iop/lib -s
IOP_LIBS += -liop_ps2ip -llwip_common
IOP_CFLAGS += -DPS2DRV_COMPAT

all: $(IOP_BIN)

clean:
	-rm -f *.o

include $(TOPDIR)/Makefile.pref
include $(TOPDIR)/Makefile.global

include $(PS2LIB)/Makefile.pref
include $(PS2LIB)/Makefile.iopglobal
