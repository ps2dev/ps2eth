/*
 * main.c - PS2 SMAP ethernet adapter driver.
 *
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * See the file LICENSE, located within this directory, for licensing terms.
 */

#include "smap.h"

IRX_ID(MODNAME, 1, 0);

#define VERSION	"1.0"
#define BANNER "\nps2smap: PS2 SMAP ethernet adapter driver - v%s\n"	\
		"Copyright (c) 2003 Marcus R. Brown\n\n"

int smap_evflg = -1;

int _start(int argc, char **argv)
{
	printf(BANNER, VERSION);

	return 0;
}

static void main_thread(void *arg)
{
	smap_state_t *state = (smap_state_t *)arg;
	u32 bits;

	while (1) {
		if (WaitEventFlag(smap_evflg, SMAP_EVENT_ALL, 0x11, &bits) != 0)
			return;

		if (bits & SMAP_EVENT_EXIT)
			smap_exit_event(state);

		if (bits & SMAP_EVENT_INIT)
			if (smap_init_event(state) != 0)
				break;

		if (bits & SMAP_EVENT_INTR)
			if (smap_interrupt_event(state))
				bits |= SMAP_EVENT_TX;

		if (bits & SMAP_EVENT_TX)
			smap_tx_event(state);
	}
}

#include "imports.lst"
