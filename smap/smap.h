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

#ifdef DEBUG
#define DPRINTF(format, args...) printf("%s: " format, __FUNCTION__ , ## args)
#else
#define DPRINTF(format, args...)
#endif

/* Event flags.  */
#define SMAP_EVENT_INIT		0x01
#define SMAP_EVENT_EXIT		0x02
#define SMAP_EVENT_INTR		0x04
#define SMAP_EVENT_TX		0x08
#define SMAP_EVENT_ALARM	0x10

#define SMAP_EVENT_ALL		(SMAP_EVENT_INIT|SMAP_EVENT_EXIT| \
				SMAP_EVENT_INTR|SMAP_EVENT_TX|SMAP_EVENT_ALARM)

/* Events handled by the interface thread.  */
#define SMAP_IF_EVENT_INCPL	0x01	/* SMAP device initialization completed.  */
#define SMAP_IF_EVENT_RECV	0x02	/* Packet data received.  */
#define SMAP_IF_EVENT_ARP	0x04	/* ARP timer expired.  */

#define SMAP_IF_EVENT_ALL	(SMAP_IF_EVENT_INCPL|SMAP_IF_EVENT_RECV| \
				SMAP_IF_EVENT_ARP)

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

	int	evflg;		/* The global event flag.  */
	int	thid;		/* The ID of the main thread.  */
	struct netif *netif;	/* lwIP network interface.  */

	int	if_evflg;	/* Used to signal the interface thread.  */

	u16	txfree;		/* Number of bytes free in the TX FIFO.  */
	int	txbdsi;		/* Saved index into TX BD.  */
	int	txbdi;		/* Index into current TX BD.  */
	int	txbd_used;	/* Keeps track of how many TX BD's have been used.  */

	int	rxbdi;		/* Index into current RX BD.  */

	struct pbuf *txq;	/* TX queue.  */
	struct pbuf *rxq;	/* RX queue.  */
	struct pbuf *lasttxp;	/* The last packet attempted for TX.  */

	int	has_init;	/* SMAP has been initialized.  */
	int	has_alarm_init;
	int	has_link;	/* Link has been established.  */
	iop_sys_clock_t timeout;

	int	no_auto;	/* Don't use autonegotiation.  */

	smap_stat_t stats;
	smap_error_t errors;
} smap_state_t;

/* Global variables (main.c)  */
extern smap_state_t smap_state;

extern struct ip_addr smap_ip, smap_sm, smap_gw;


/* SMAP-specific routines (smap.c)  */

int smap_reset(smap_state_t *state);

void smap_thread(void *arg);


/* LwIP and ARP interfaces (smapif.c)  */

int smap_if_init(smap_state_t *state);

/* Add a pbuf to the queue.  */
void smap_p_enqueue(struct pbuf **pq, struct pbuf *p);
/* Grab the next single pbuf from the queue.  */
struct pbuf *smap_p_next(struct pbuf **pq);
/* Empty the queue.  */
struct pbuf *smap_p_dequeue(struct pbuf **pq);

#endif	/* SMAP_H */
