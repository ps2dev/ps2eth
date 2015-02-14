#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#                           (C)2003, adresd     ( adresd_ps2dev@yahoo.com )
#  ------------------------------------------------------------------------


IOP_BIN = ps2smap.irx
IOP_OBJS = main.o smap.o xfer.o imports.o

# Uncomment the line below to build for use with a DHCP-enabled LWIP stack.
#LWIP_DHCP=1

IOP_INCS += -I$(PS2SDK)/iop/include
IOP_CFLAGS += -Wall -fno-builtin -D_IOP -mgpopt -G8192
IOP_LDFLAGS += -s

ifdef LWIP_DHCP
IOP_CFLAGS  += -DLWIP_DHCP=1
endif

all: $(IOP_BIN)

clean:
	rm -f $(IOP_OBJS)

include $(PS2SDK)/Defs.make
include Rules.make
