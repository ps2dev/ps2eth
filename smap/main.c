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

struct ip_addr smap_ip, smap_sm, smap_gw;

smap_state_t smap_state;

static void main_thread(void *arg);
static int parse_options(int argc, char **argv, smap_state_t *state);
static int show_usage(void);

int _start(int argc, char **argv)
{
	iop_thread_t thread;
	iop_event_t event;
	smap_state_t *state = &smap_state;
	u8 *hwaddr = (u8 *)&state->eth_addr;
	int res;

	printf(BANNER, VERSION);

	if (!parse_options(argc, argv, state))
		return show_usage();

	if ((res = smap_reset(state)) != 0) {
		M_PRINTF("Error: Unable to reset SMAP, error code %d, exiting.\n", res);
		return 1;
	}

	M_PRINTF("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n", hwaddr[0], hwaddr[1],
			hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);

	/* Create the global event flag and start the main thread.  */
	event.attr = 0;
	event.bits = 0;
	if ((state->evflg = CreateEventFlag(&event)) < 0) {
		M_PRINTF("Error: Couldn't create event flag, exiting.\n");
		return 1;
	}

	thread.attr = TH_C;
	thread.option = 0;
	thread.thread = main_thread;
	thread.stacksize = 8192;
	thread.priority = 40;
	if ((state->thid = CreateThread(&thread)) < 0) {
		M_PRINTF("Error: Unable to create main thread, exiting.\n");
		goto error;
	}
	if (StartThread(state->thid, state) < 0) {
		M_PRINTF("Error: Couldn't execute main thread, exiting.\n");
		goto error;
	}

	/* Now that the main thread is waiting for events, attempt to register
	   the ps2ip interface and complete SMAP initialization.  */
	if ((res = smap_if_init(state)) != 0) {
		M_PRINTF("Error: Unable to start SMAP interface, error code %d, " \
				"exiting.\n", res);
		goto error;
	}

	M_PRINTF("Driver initialized and ready.\n");
	return 0;

error:
	if (state->thid > 0) {
		TerminateThread(state->thid);
		DeleteThread(state->thid);
	}
	if (state->evflg > 0)
		DeleteEventFlag(state->evflg);
	
	return 1;
}

static void main_thread(void *arg)
{
	smap_state_t *state = (smap_state_t *)arg;
	int res;
	u32 bits;

	state->txbp = SMAP_TX_BUFSIZE;

	while (1) {
		if ((res = WaitEventFlag(state->evflg, SMAP_EVENT_ALL, 0x11, &bits)) != 0)
			return;

		if (bits & SMAP_EVENT_EXIT)
			smap_exit_event(state);

		if (bits & SMAP_EVENT_INIT) {
			smap_init_event(state);

			if (!state->has_init)
				continue;
		}

		if (bits & SMAP_EVENT_INTR)
			if (smap_interrupt_event(state))
				smap_tx_event(state, NULL);
	}
}

static int parse_options(int argc, char **argv, smap_state_t *state)
{
	/* Set some defaults.  */
	IP4_ADDR(&smap_ip, 192, 168, 1, 23);
	IP4_ADDR(&smap_sm, 255, 255, 255, 0);
	IP4_ADDR(&smap_gw, 192, 168, 1, 254);

	return 1;
}

static int show_usage()
{
	printf("Usage: ps2smap [ip address] [subnet mask] [gateway address]\n\t" \
		"[-noauto/-auto]\n");
	return 1;
}

#include "imports.lst"
