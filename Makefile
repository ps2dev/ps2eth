#  _____     ___ ____
#   ____|   |    ____|      PSX2 OpenSource Project
#  |     ___|   |____       (C)2002, David Ryan ( Oobles@hotmail.com )
#  ------------------------------------------------------------------------


all: 
	$(MAKE) -C common
	$(MAKE) -C smap
	$(MAKE) -C ps2klsi


clean:
	$(MAKE) -C bin clean
	$(MAKE) -C common clean
	$(MAKE) -C smap clean
	$(MAKE) -C ps2klsi clean
