# Makefile for SMAP ethernet adapter driver.
# Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
# See the file LICENSE, located within this directory, for licensing terms.

TOPDIR = ..

IOP_BIN = ../bin/ps2smap.irx
IOP_OBJS = main.o smapif.o smap.o imports.o

ifeq ($(DEBUG),1)
DEBUG_FLAGS = -DDEBUG
endif

IOP_CFLAGS += -Wall -fno-builtin $(DEBUG_FLAGS)
IOP_LDFLAGS += -L$(PS2IP)/iop/lib -s

all: $(IOP_BIN)

include $(TOPDIR)/Makefile.pref
include $(TOPDIR)/Makefile.global

include $(PS2DRV)/Defs.make
include $(PS2DRV)/iop/Rules.make
