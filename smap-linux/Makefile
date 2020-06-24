#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#                           (C)2003, adresd     ( adresd_ps2dev@yahoo.com )
#  ------------------------------------------------------------------------


IOP_BIN = ps2smap.irx
IOP_OBJS = main.o smap.o imports.o

# Uncomment the line below to build for use with a DHCP-enabled LWIP stack.
#LWIP_DHCP=1

ifeq ($(DEBUG),1)
DEBUG_FLAGS = -DDEBUG
endif

IOP_INCS += -I$(PS2SDK)/iop/include
IOP_CFLAGS += -Wall -fno-builtin $(DEBUG_FLAGS)
IOP_LDFLAGS += -s

ifdef LWIP_DHCP
IOP_CFLAGS  += -DLWIP_DHCP=1
endif

all: $(IOP_BIN)

clean:
	-rm -f $(IOP_BIN) $(IOP_OBJS)

include $(PS2SDK)/Defs.make
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
