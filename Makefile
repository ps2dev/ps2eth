#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------

.PHONY: all clean install

all: 
	$(MAKE) -C common
	$(MAKE) -C ps2klsi
	$(MAKE) -C smap
	$(MAKE) -C smap-linux

clean:
	$(MAKE) -C common clean
	$(MAKE) -C ps2klsi clean
	$(MAKE) -C smap clean
	$(MAKE) -C smap-linux clean

install: all
	mkdir -p $(PS2DEV)/ps2eth/smap
	cp smap/ps2smap.irx $(PS2DEV)/ps2eth/smap/
	mkdir -p $(PS2DEV)/ps2eth/ps2klsi
	cp ps2klsi/ps2klsi.irx $(PS2DEV)/ps2eth/ps2klsi/
	mkdir -p $(PS2SDK)/iop/irx/
	ln -sf $(PS2DEV)/ps2eth/smap/ps2smap.irx $(PS2SDK)/iop/irx/
	ln -sf $(PS2DEV)/ps2eth/ps2klsi/ps2klsi.irx $(PS2SDK)/iop/irx/
