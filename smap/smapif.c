/*
 * smapif.c - High level ps2ip/LwIP interface.
 *
 * Copyright (c) 2003 Marcus R. Brown <mrbrown@0xd6.org>
 *
 * See the file LICENSE, located within this directory, for licensing terms.
 */

#include "smap.h"

extern err_t tcpip_input(struct pbuf *p, struct netif *inp);

static err_t smap_if_start(struct netif *netif);
static err_t smap_if_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr);
static err_t smap_if_linkoutput(struct netif *netif, struct pbuf *p);

static void smap_if_thread(void *arg);

int smap_if_init(smap_state_t *state)
{
	iop_thread_t thread;
	iop_event_t event;
	struct netif *netif;
	int thid;

	event.attr = 0;
	event.bits = 0;
	if ((state->if_evflg = CreateEventFlag(&event)) < 0) {
		M_PRINTF("Error: Couldn't create interface event flag.\n");
		return 2;
	}

	thread.attr = TH_C;
	thread.option = 0;
	thread.thread = smap_if_thread;
	thread.stacksize = 4096;
	thread.priority = 42;
	if ((thid = CreateThread(&thread)) < 0) {
		M_PRINTF("Error: Unable to create interface thread.\n");
		return 3;
	}
	if (StartThread(thid, state) < 0) {
		M_PRINTF("Error: Couldn't execute interface thread.\n");
		return 4;
	}

	if (!(netif = netif_add(&smap_ip, &smap_sm, &smap_gw, state,
				smap_if_start, tcpip_input))) {
		M_PRINTF("Couldn't add SMAP interface to ps2ip.\n");
		return 5;
	}

	netif_set_default(netif);
	return 0;
}

void smap_if_input(smap_state_t *state)
{
	struct netif *netif = state->netif;
	struct eth_hdr *eth_hdr;
	struct pbuf *p = smap_p_dequeue(&state->rxq);

	DPRINTF("enter, p is %p\n", p);
	eth_hdr = p->payload;
	switch (htons(eth_hdr->type)) {
		case ETHTYPE_IP:
			DPRINTF("IP\n");
			etharp_ip_input(netif, p);
			pbuf_header(p, -14);
			netif->input(p, netif);
			break;
		case ETHTYPE_ARP:
			DPRINTF("ARP\n");
			etharp_arp_input(state->netif, &state->eth_addr, p);
			break;
		default:
			pbuf_free(p);
			break;
	}
	DPRINTF("exit\n");
}

static u32 etharp_alarm_cb(void *arg)
{
	iSetEventFlag(smap_state.if_evflg, SMAP_IF_EVENT_ARP);
	return *(u32 *)arg;
}

void smap_if_thread(void *arg)
{
	iop_sys_clock_t arp_timer;
	smap_state_t *state = (smap_state_t *)arg;
	u32 bits;
	int res, arp_init = 0;

	while (1) {
		if ((res = WaitEventFlag(state->if_evflg, SMAP_IF_EVENT_ALL, 0x11, &bits)))
			break;

		/* Packet(s) received.  */
		if (bits & SMAP_IF_EVENT_RECV)
			smap_if_input(state);

		/* ARP timer expired.  */
		if (bits & SMAP_IF_EVENT_ARP)
			etharp_tmr();

		/* The device has finished initialization.  */
		if (bits & SMAP_IF_EVENT_INCPL) {
			DPRINTF("Got init completion event.\n");

			/* Initialize the ARP interface.  */
			if (!arp_init) {
				etharp_init();
				USec2SysClock(ARP_TMR_INTERVAL * 1000, &arp_timer);
				SetAlarm(&arp_timer, etharp_alarm_cb, &arp_timer.lo);
				arp_init = 1;
			}
		}

	}
	DPRINTF("WaitEventFlag returned %d\n", res);
}

static err_t smap_if_start(struct netif *netif)
{
	smap_state_t *state = (smap_state_t *)netif->state;

	/* Setup the network interface.  */
	state->netif = netif;
	netif->output = smap_if_output;
	netif->linkoutput = smap_if_linkoutput;
	netif->name[0] = 's';
	netif->name[1] = 'm';

	/* Copy the MAC address (we have this from smap_reset()) */
	netif->hwaddr_len = 6;
	memcpy(netif->hwaddr, &state->eth_addr, netif->hwaddr_len);

	/* Start the SMAP device.  */
	SetEventFlag(state->evflg, SMAP_EVENT_INIT);
	return 0;
}

static err_t smap_if_output(struct netif *netif, struct pbuf *p, struct ip_addr *ipaddr)
{
	if ((p = etharp_output(netif, ipaddr, p)))
		return smap_if_linkoutput(netif, p);

	return 0;
}

static err_t smap_if_linkoutput(struct netif *netif, struct pbuf *p)
{
	smap_state_t *state = (smap_state_t *)netif->state;

	smap_p_enqueue(&state->txq, p);
	SetEventFlag(state->evflg, SMAP_EVENT_TX);
	return 0;
}

static int priority_one()
{
	int flags;
	CpuSuspendIntr(&flags);
	return flags;
}

static void priority_reset(int priority)
{
	CpuResumeIntr(priority);
}

void smap_p_enqueue(struct pbuf **pq, struct pbuf *p)
{
	struct pbuf *q;
	int oldpri;

	if ((oldpri = priority_one()) < 0)
		return;

	if (!(*pq)) {
		*pq = p;
	} else {
		for (q = *pq; q->next != NULL; q = q->next)
			;
		q->next = p;
	}
	/* In both cases, the reference count must be incremented so that if
	   the high-level code gets a chance to free it before smap_transmit()
	   does, we still have valid data to send.  */
	pbuf_ref(p);

	priority_reset(oldpri);
}

struct pbuf *smap_p_next(struct pbuf **pq)
{
	struct pbuf *head;
	int oldpri;

	if ((oldpri = priority_one()) < 0)
		return NULL;

	if ((head = *pq)) {
		*pq = head->next;
		head->next = NULL;
	}

	priority_reset(oldpri);
	return head;
}

struct pbuf *smap_p_dequeue(struct pbuf **pq)
{
	struct pbuf *head, *q;
	int oldpri;

	if ((oldpri = priority_one()) < 0)
		return NULL;

	/* Decrement the refcount on each pbuf (undo smap_p_enqueue()).  */
	if ((head = *pq)) {
		for (q = head; q; q = q->next)
			pbuf_free(q);
	}
	*pq = NULL;

	priority_reset(oldpri);
	return head;
}
