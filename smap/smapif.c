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

static void arp_thread(void *arg);

int smap_if_init(smap_state_t *state)
{
	iop_thread_t thread;
	iop_event_t event;
	struct netif *netif;
	int thid;

	event.attr = 0;
	event.bits = 0;
	if ((state->arp_evflg = CreateEventFlag(&event)) < 0) {
		M_PRINTF("Error: Couldn't create ARP event flag.\n");
		return 2;
	}

	thread.attr = TH_C;
	thread.option = 0;
	thread.thread = arp_thread;
	thread.stacksize = 4096;
	thread.priority = 39;
	if ((thid = CreateThread(&thread)) < 0) {
		M_PRINTF("Error: Unable to create ARP thread.\n");
		return 3;
	}
	if (StartThread(thid, state) < 0) {
		M_PRINTF("Error: Couldn't execute ARP thread.\n");
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

void smap_if_input(smap_state_t *state, struct pbuf *p)
{
	struct netif *netif = state->netif;
	struct eth_hdr *eth_hdr = p->payload;

	switch (htons(eth_hdr->type)) {
		case ETHTYPE_IP:
			etharp_ip_input(netif, p);
			pbuf_header(p, -14);
			netif->input(p, netif);
			break;
		case ETHTYPE_ARP:
			state->arp_pbuf = p;
			SetEventFlag(state->arp_evflg, SMAP_ARP_EVENT_IN);
			break;
		default:
			pbuf_free(p);
			break;
	}
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
	int res;

	/* Transmit the packets.  */
	res = smap_tx_event(state, p);
	return res == 1 ? 0 : -11 /* ERR_IF */;
}

static int etharp_alarm_cb(void *arg)
{
	iSetEventFlag(smap_state.arp_evflg, SMAP_ARP_EVENT_TMR);
	return *(u32 *)arg;
}

static void arp_thread(void *arg)
{
	iop_sys_clock_t arp_timer;
	smap_state_t *state = (smap_state_t *)arg;
	struct pbuf *p;
	u32 bits;

	etharp_init();
	USec2SysClock(ARP_TMR_INTERVAL * 1000, &arp_timer);
	SetAlarm(&arp_timer, etharp_alarm_cb, &arp_timer.lo);

	while (1) {
		WaitEventFlag(state->arp_evflg, SMAP_ARP_EVENT_ALL, 0x11, &bits);

		if (bits & SMAP_ARP_EVENT_IN) {
			if ((p = etharp_arp_input(state->netif,
					&state->eth_addr, state->arp_pbuf))) {
				smap_if_linkoutput(state->netif, p);
				pbuf_free(p);
			}
		}

		if (bits & SMAP_ARP_EVENT_TMR)
			etharp_tmr();
	}
}
