IOP_SRC_DIR = ./
TOPDIR = ../..

IOP_LIB = liblwip_common.a
IOP_OBJS = etharp.o

all: $(IOP_LIB)

clean:
	-rm -r -f $(IOP_BIN) $(IOP_OBJS) $(IOP_OBJS_DIR)

include $(TOPDIR)/Makefile.global

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
