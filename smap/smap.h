/*
 * smap.h - Header file for the SMAP ethernet adapter driver.
 *
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * See the file LICENSE, located within this directory, for licensing terms.
 */

#ifndef SMAP_H
#define	SMAP_H

#include "defs.h"
#include "types.h"
#include "irx.h"
#include "loadcore.h"
#include "intrman.h"
#include "stdio.h"
#include "sysclib.h"
#include "thbase.h"
#include "thsemap.h"
#include "thevent.h"
#include "dev9.h"
#include "speedregs.h"
#include "smapregs.h"

#include "lwip/ip.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"

#include "netif/etharp.h"

#define MODNAME "ps2smap"

#define M_PRINTF(format, args...)	\
	printf(MODNAME ": " format, ## args)

/* Event flags.  */
#define SMAP_EVENT_INIT		0x01
#define SMAP_EVENT_EXIT		0x02
#define SMAP_EVENT_INTR		0x04
#define SMAP_EVENT_TX		0x08
#define SMAP_EVENT_ALARM	0x10

#define SMAP_EVENT_ALL		(SMAP_EVENT_INIT|SMAP_EVENT_EXIT|\
				SMAP_EVENT_INTR|SMAP_EVENT_TX|SMAP_EVENT_ALARM)

/* Directions for DMA transfers.  */
#define SMAP_DMA_IN	0
#define SMAP_DMA_OUT	1

/* Various status/error codes that we accumulate.  */
typedef struct {
	
} smap_stat_t;

/* The current state of the SMAP device.  This is attached to the netif
   structure and passed between most SMAP-specific routines.  */
typedef struct {
	u8	hwaddr[6];	/* MAC address.  */

	u16	txbp;		/* Pointer into the TX buffer.  */
	int	txbdi;		/* Index into current TX BD.  */
	int	txbd_used;	/* Keeps track of how many TX BD's have been used.  */
	struct pbuf *tx_pbuf;	/* This is passed in from the ps2ip interface.  */
	u16	tx_plen;	/* This is passed in from the ps2ip interface.  */

	u16	rxbp;		/* Pointer into the RX buffer.  */
	int	rxbdi;		/* Index into current RX BD.  */

	smap_stat_t stat;
} smap_state_t;

/* Global variables (main.c)  */
extern int smap_evflg;


/* SMAP-specific routines (smap.c)  */

int smap_reset(smap_state_t *state);

int smap_init_event(smap_state_t *state);
void smap_exit_event(smap_state_t *state);
int smap_interrupt_event(smap_state_t *state);
void smap_tx_event(smap_state_t *state);


/* LwIP and ARP interfaces (smapif.c)  */

int smap_if_init(smap_state_t *state);

#endif	/* SMAP_H */
