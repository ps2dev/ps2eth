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

#define SMAP_EVENT_ALL		(SMAP_EVENT_INIT|SMAP_EVENT_EXIT|SMAP_EVENT_INTR)

/* ARP events.  */
#define SMAP_ARP_EVENT_IN	0x01	/* ARP input.  */
#define SMAP_ARP_EVENT_TMR	0x02	/* ARP timer.  */

#define SMAP_ARP_EVENT_ALL	(SMAP_ARP_EVENT_IN|SMAP_ARP_EVENT_TMR)

/* Directions for DMA transfers.  */
#define SMAP_DMA_IN	0
#define SMAP_DMA_OUT	1

/* Various statistics that we accumulate.  */
typedef struct {
	
} smap_stat_t;

/* We use this to keep track of errors.  */
typedef struct {
	
} smap_error_t;

/* The current state of the SMAP device.  This is attached to the netif
   structure and passed between most SMAP-specific routines.  */
typedef struct {
	struct eth_addr eth_addr; /* MAC address.  */
	u16	checksum;	/* Checksum of the MAC address.  */

	int	has_init;	/* Has SMAP been initialized?  */
	int	evflg;		/* The global event flag.  */
	int	thid;		/* The ID of the main thread.  */
	struct netif *netif;	/* lwIP network interface.  */

	int	arp_evflg;	/* Used to signal the ARP thread.  */
	struct pbuf *arp_pbuf;	/* Data passed into the ARP thread.  */

	u16	txbp;		/* Pointer into the TX buffer.  */
	int	txbdsi;		/* Saved index into TX BD.  */
	int	txbdi;		/* Index into current TX BD.  */
	int	txbd_used;	/* Keeps track of how many TX BD's have been used.  */

	u16	rxbp;		/* Pointer into the RX buffer.  */
	int	rxbdi;		/* Index into current RX BD.  */

	int	no_auto;	/* Don't use autonegotiation.  */

	smap_stat_t stats;
	smap_error_t errors;
} smap_state_t;

/* Global variables (main.c)  */
extern smap_state_t smap_state;

extern struct ip_addr smap_ip, smap_sm, smap_gw;


/* SMAP-specific routines (smap.c)  */

int smap_reset(smap_state_t *state);

int smap_init_event(smap_state_t *state);
void smap_exit_event(smap_state_t *state);
int smap_interrupt_event(smap_state_t *state);
int smap_tx_event(smap_state_t *state, struct pbuf *p);


/* LwIP and ARP interfaces (smapif.c)  */

int smap_if_init(smap_state_t *state);
void smap_if_input(smap_state_t *state, struct pbuf *p);

#endif	/* SMAP_H */
