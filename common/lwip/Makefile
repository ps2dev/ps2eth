
TOPDIR = ../..

IOP_LIB = liblwip_common.a
IOP_OBJS = etharp.o

all: $(IOP_LIB)

clean:
	-rm -f $(IOP_OBJS) $(IOP_LIB)

include $(TOPDIR)/Makefile.global

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
