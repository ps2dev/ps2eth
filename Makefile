#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------


all: 
	$(MAKE) -C common
	$(MAKE) -C smap
	$(MAKE) -C smap-linux

clean:
	$(MAKE) -C bin clean
	$(MAKE) -C common clean
	$(MAKE) -C smap clean
	$(MAKE) -C smap-linux clean

install:
	mkdir -p $(PS2DEV)/ps2eth/smap
	cp smap/ps2smap.irx $(PS2DEV)/ps2eth/smap/
	mkdir -p $(PS2SDK)/iop/irx/
	ln -s $(PS2DEV)/ps2eth/smap/ps2smap.irx $(PS2SDK)/iop/irx/