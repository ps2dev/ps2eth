#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#                           (C)2003, adresd     ( adresd_ps2dev@yahoo.com )
#  ------------------------------------------------------------------------

PS2DRV=$(PS2DEV)/ps2drv

IOP_BIN = ps2smap.irx
IOP_OBJS = main.o smap.o imports.o

ifeq ($(DEBUG),1)
DEBUG_FLAGS = -DDEBUG
endif

IOP_INCS += -I$(PS2DRV)/iop/include
IOP_CFLAGS += -Wall -fno-builtin $(DEBUG_FLAGS)
IOP_LDFLAGS += -s

all: $(IOP_BIN)

include $(PS2DRV)/Defs.make
include $(PS2DRV)/iop/Rules.make
