/*
  _____     ___ ____
   ____|   |    ____|      PS2 OpenSource Project
  |     ___|   |____       (C)2002, David Ryan ( oobles@hotmail.com )
  ------------------------------------------------------------------------
  ps2klsi.c                Kawasaki USB Ethernet driver.

  This is the driver ethernet driver for a NetGear 101 usb device.  It
  is based on usbd.irx for usb access, and is designed for ps2ip.irx to
  allow TCP/IP access over the device.

  ps2eth is based on the lwip standard ethernet code and adapted for
  the Kawasaki chipset.  There isn't too many changes here.
*/

#include <tamtypes.h>
#include <thbase.h>

#include "lwip/pbuf.h"

#include "netif/etharp.h"


#include "ps2eth.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'e'
#define IFNAME1 't'

struct ethernetif klsi_etherif;

static const struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};

/* Forward declarations. */
static void  ethernetif_input(struct netif *netif, char *buf, u16_t len);
static err_t ethernetif_output(struct netif *netif, struct pbuf *p,
			       struct ip_addr *ipaddr);


extern u8_t eaddr[6];  //read_mac uses this for the mac address.

static char inbuffer[1500] __attribute__((aligned(64)));

void readthread( struct netif *netif )
{
   struct ethernetif *ethernetif = &klsi_etherif;
   int bytesread;
   printf( "PS2KLSI: Read thread started.\n" );

   while(1)
   {
      
      bytesread = kue_do_transfer( ethernetif->hin, ethernetif->hsemin, inbuffer, 1500 ); 
      if (bytesread > 1 )
      {
          //lock_printf( "got bytes: %i\n", bytesread );
          ethernetif_input( netif, inbuffer, bytesread ); 
      }
 
      
   }

}


static void
low_level_init(struct netif *netif)
{
  struct ethernetif *ethernetif;
  struct t_thread t;
  int tid;

  ethernetif = netif->state;
  
  /* Obtain MAC address from network interface. */
  //
  ethernetif->ethaddr->addr[0] = eaddr[0];
  ethernetif->ethaddr->addr[1] = eaddr[1];
  ethernetif->ethaddr->addr[2] = eaddr[2];
  ethernetif->ethaddr->addr[3] = eaddr[3];
  ethernetif->ethaddr->addr[4] = eaddr[4];
  ethernetif->ethaddr->addr[5] = eaddr[5];

  /* Do whatever else is needed to initialize interface. */  
  // Open the endpoints..
  // done in attach for simplicity.

  // create semaphores.
  ethernetif->semin.attr = 0;
  ethernetif->semin.option = 0;
  ethernetif->semin.init_count = 0;
  ethernetif->semin.max_count = 1;
  ethernetif->hsemin = CreateSema( &(ethernetif->semin) );

  ethernetif->semout.attr = 0;
  ethernetif->semout.option = 0;
  ethernetif->semout.init_count = 0;
  ethernetif->semout.max_count = 1;
  ethernetif->hsemout = CreateSema( &(ethernetif->semout) );

  // Start the read thread..
 
  t.type = 0x02000000;
  t.unknown = 0;
  t.function = readthread;
  t.stackSize = 0x800;
  t.priority = 0x1e;
  tid = CreateThread( &t );
  if ( tid >= 0 )
     StartThread( tid, netif );
  

}

/*-----------------------------------------------------------------------------------*/
/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 */
/*-----------------------------------------------------------------------------------*/

static char outbuffer[1500] __attribute((aligned(64)));

static err_t
low_level_output(struct ethernetif *ethernetif, struct pbuf *p)
{
  struct pbuf *q;
  char *bufptr;
  u16_t length = 0;

  // leave two bytes for packet length at start of buffer.
  bufptr = &outbuffer[2];

 
  for(q = p; q != NULL; q = q->next) {
    /* Send the data from the pbuf to the interface, one pbuf at a
       time. The size of the data in each pbuf is kept in the ->len
       variable. */
    bcopy( q->payload, bufptr, q->len );
    bufptr += q->len;
    length += q->len;
  }

  // padd buffer out to 64 byte boundry.
  length += 64 - (length % 64 ); 

  // add two character for packet length;
  // This is specific to the Kawasaki USB interface.

  length = p->tot_len + 2;

  outbuffer[0] = (u8_t) (p->tot_len);
  outbuffer[1] = (u8_t) (p->tot_len >> 8);

  kue_do_transfer( ethernetif->hout, ethernetif->hsemout, outbuffer, p->tot_len+2 ); 

  return 0;
}
/*-----------------------------------------------------------------------------------*/
/*
 * low_level_input():
 *
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 */
/*-----------------------------------------------------------------------------------*/
static struct pbuf *
low_level_input(struct ethernetif *ethernetif, char * buf, u16_t len )
{
  struct pbuf *p, *q;
  char *bufptr;
  u8_t  b1,b2; 
  u16_t length;

  len-=2;
  b1 = buf[0];
  b2 = buf[1];
  length = (b2 << 8) | b1;
  len = length;


  /* We allocate a pbuf chain of pbufs from the pool. */
  p = pbuf_alloc(PBUF_LINK, len, PBUF_POOL);
 

  if(p != NULL) {
    /* We iterate over the pbuf chain until we have read the entire
       packet into the pbuf. */
    bufptr = &buf[2];   // First two characters are size.
    for(q = p; q != NULL; q = q->next) {
      /* Read enough bytes to fill this pbuf in the chain. The
         avaliable data in the pbuf is given by the q->len
         variable. */
      bcopy( bufptr, q->payload, q->len );
      bufptr += q->len;
    }

  }
  return p;  
}
/*-----------------------------------------------------------------------------------*/
/*
 * ethernetif_output():
 *
 * This function is called by the TCP/IP stack when an IP packet
 * should be sent. It calls the function called low_level_output() to
 * do the actuall transmission of the packet.
 *
 */
/*-----------------------------------------------------------------------------------*/
static err_t
ethernetif_output(struct netif *netif, struct pbuf *p,
		  struct ip_addr *ipaddr)
{
  p = etharp_output( netif, ipaddr, p);
  if ( p!= NULL ) {
     return low_level_output( netif->state, p );
  }
  return 0;

}
/*-----------------------------------------------------------------------------------*/
/*
 * ethernetif_input():
 *
 * This function should be called when a packet is ready to be read
 * from the interface. It uses the function low_level_input() that
 * should handle the actual reception of bytes from the network
 * interface.
 *
 */
/*-----------------------------------------------------------------------------------*/
static void
ethernetif_input(struct netif *netif, char *buf, u16_t len)
{
  struct ethernetif *ethernetif;
  struct eth_hdr *ethhdr;
  struct pbuf *p;



  ethernetif = netif->state;
  
  p = low_level_input(ethernetif, buf, len );

  if(p != NULL) {

    ethhdr = p->payload;


    switch(htons(ethhdr->type)) {
    case ETHTYPE_IP:
      etharp_ip_input(netif, p);
      pbuf_header(p, -14);
      netif->input(p, netif);
      break;
    case ETHTYPE_ARP:
      p = etharp_arp_input(netif, ethernetif->ethaddr, p);
      if(p != NULL) {
	low_level_output(ethernetif, p);
	pbuf_free(p);
      }
      break;
    default:
      pbuf_free(p);
      break;
    }
  }
}
/*-----------------------------------------------------------------------------------*/
static void
arp_timer(void *arg)
{
  etharp_tmr();
  //sys_timeout(ARP_TMR_INTERVAL, (sys_timeout_handler)etharp_timer, NULL);
}
/*-----------------------------------------------------------------------------------*/
/*
 * ethernetif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */
/*-----------------------------------------------------------------------------------*/


void
klsi_init(struct netif *netif)
{
  struct ethernetif *ethernetif;
    
  ethernetif = &klsi_etherif; //mem_malloc(sizeof(struct ethernetif));
  netif->state = ethernetif;
  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->output = ethernetif_output;
  netif->linkoutput = low_level_output;
  
  ethernetif->ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);
  
  low_level_init(netif);
  etharp_init();

  //sys_timeout(ARP_TMR_INTERVAL, (sys_timeout_handler)arp_timer, NULL);
}
/*-----------------------------------------------------------------------------------*/
