#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#
#  ------------------------------------------------------------------------

TOPDIR = ..

IOP_BIN = ps2klsi.irx
IOP_OBJS = driver.o ps2eth.o imports.o

IOP_LIBS += -llwip_common


all: $(IOP_BIN)

clean:
	rm -f $(IOP_BIN) $(IOP_OBJS)

include $(TOPDIR)/Makefile.global

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
