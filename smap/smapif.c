/*
 * Copyright (c) 2001, Swedish Institute of Computer Science.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id$
 */

#include "lwip/debug.h"

#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/ip.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"

#include "netif/etharp.h"
#include "sysclib.h"
#include "smap.h"


#define IFNAME0 's'
#define IFNAME1 'm'

static const struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};

struct smapif {
    struct eth_addr *ethaddr;
    /* Add whatever per-interface state that is needed here. */
};

struct smapif smap_etherif;

static struct netif *this_netif;

/* Forward declarations. */
static void  smapif_input(struct netif *netif, char*buf, int len);
static err_t smapif_output(struct netif *netif, struct pbuf *p,
			       struct ip_addr *ipaddr);

/*
 * This is called when data is read from the driver
 * (wrapper so smap wont need to know about netif)
 */
void low_level_input(char *buf, int len)
{
	smapif_input((struct netif*)this_netif,buf,len);	
}


static void
low_level_init(struct netif *netif)
{
  struct smapif *smapif;

  smapif = netif->state;

  /* Get MAC address */
  memcpy((unsigned char *)smapif->ethaddr, smap_get_mac(), 6);
  //  smap_get_mac_address((unsigned char *)smapif->ethaddr->addr);
#if 0
  printf("MAC address : %02X:%02X:%02X:%02X:%02X:%02X\n",
		  (unsigned char)(smapif->ethaddr->addr[0]),	
		  (unsigned char)(smapif->ethaddr->addr[1]),	
		  (unsigned char)(smapif->ethaddr->addr[2]),	
		  (unsigned char)(smapif->ethaddr->addr[3]),	
		  (unsigned char)(smapif->ethaddr->addr[4]),	
		  (unsigned char)(smapif->ethaddr->addr[5]));
#endif  
  smap_start();
}

/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 */

static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
#if 0
    struct pbuf *q;
    char buf[1600];
    char *bufptr;
  
    /* initiate transfer(); */
    bufptr = &buf[0];
  
    for(q = p; q != NULL; q = q->next) {
        /* Send the data from the pbuf to the interface, one pbuf at a
           time. The size of the data in each pbuf is kept in the ->len
           variable. */    
        /* send data from(q->payload, q->len); */
        bcopy(q->payload, bufptr, q->len);
        bufptr += q->len;
    }
    
    /* signal that packet should be sent(); */
    smap_send(buf, p->tot_len);
#else
    // Moved pbuf handling to smap_send() (eliminating one set of memcpy's)
    smap_send(p, p->tot_len);
#endif
    return ERR_OK;
}

/*----------------------------------------------------------------------*/
/*
 * smapif_output():
 *
 * This function is called by the TCP/IP stack when an IP packet
 * should be sent. It calls the function called low_level_output() to
 * do the actuall transmission of the packet.
 *
 */
/*----------------------------------------------------------------------*/
static err_t
smapif_output(struct netif *netif, struct pbuf *p,
		  struct ip_addr *ipaddr)
{
    p = etharp_output(netif, ipaddr, p);
    if(p != NULL) {
        return low_level_output(netif, p);
    }
    return ERR_OK;
}


/*
 * smapif_input():
 * This function is called when the driver has
 * has some data to pass up to lwIP.
 * It does it through lwip_input.
 */

static void
smapif_input(struct netif *netif, char * bufptr, int len)
{
    struct smapif *smapif;
    struct eth_hdr *ethhdr;
    struct pbuf *p, *q;

    smapif = netif->state;
    /* We allocate a pbuf chain of pbufs from the pool. */
    p = pbuf_alloc(PBUF_LINK, len, PBUF_POOL);
  
    if(p != NULL) {
        /* We iterate over the pbuf chain until we have read the entire
           packet into the pbuf. */
        for(q = p; q != NULL; q = q->next) {
            /* Read enough bytes to fill this pbuf in the chain. The
               avaliable data in the pbuf is given by the q->len
               variable. */
            /* read data into(q->payload, q->len); */
            bcopy(bufptr, q->payload, q->len);
            bufptr += q->len;
        }
        /* acknowledge that packet has been read(); */
    } else {
        /* drop packet(); */
    }

    if(p == NULL) {
        DEBUGF(SMAPIF_DEBUG, ("smapif_input: low_level_input returned NULL\n"));
        return;
    }
    ethhdr = p->payload;

    switch(htons(ethhdr->type)) {
    case ETHTYPE_IP:
        DEBUGF(SMAPIF_DEBUG, ("smapif_input: IP packet\n"));
        etharp_ip_input(netif, p);
        pbuf_header(p, -14);
#ifdef LWIP_DEBUG    
        if(ip_lookup(p->payload, netif)) {
#endif	    
            netif->input(p, netif);
#ifdef LWIP_DEBUG    
        } else {
            printf("smapif_input: lookup failed!\n");
        }
#endif	    
        break;
    case ETHTYPE_ARP:
        DEBUGF(SMAPIF_DEBUG, ("smapif_input: ARP packet\n"));
        p = etharp_arp_input(netif, smapif->ethaddr, p);
        if(p != NULL) {
            DEBUGF(SMAPIF_DEBUG, ("smapif_input: Sending ARP reply\n"));
            low_level_output(netif, p);
            pbuf_free(p);
        }
        break;
    default:
        pbuf_free(p);
        break;
    }
}
/*----------------------------------------------------------------------*/
static void
arp_timer(void *arg)
{
  etharp_tmr();
  //  sys_timeout(ARP_TMR_INTERVAL, (sys_timeout_handler)arp_timer, NULL);
}
/*----------------------------------------------------------------------*/
/*
 * smapif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */
/*----------------------------------------------------------------------*/
void
smapif_init(struct netif *netif)
{
  struct smapif *smapif;

  this_netif = netif;  
  smapif = &smap_etherif; //mem_malloc(sizeof(struct smapif));
  netif->state = smapif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->output = smapif_output;
  netif->linkoutput = low_level_output;
  
  smapif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);

  low_level_init(netif);
  etharp_init();
  
  // XXX: Fix arp timer (easily done w alarm)
  //  sys_timeout(ARP_TMR_INTERVAL, (sys_timeout_handler)arp_timer, NULL);
}
/*----------------------------------------------------------------------*/
