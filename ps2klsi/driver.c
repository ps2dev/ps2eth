/*
  _____     ___ ____
   ____|   |    ____|      PS2 OpenSource Project
  |     ___|   |____       (C)2002, David Ryan ( oobles@hotmail.com )
  ------------------------------------------------------------------------
  ps2klsi.c                Kawasaki USB Ethernet driver.

  This is the driver ethernet driver for a NetGear 101 usb device.  It
  is based on usbd.irx for usb access, and is designed for ps2ip.irx to
  allow TCP/IP access over the device.

  This driver was based on the BSD driver written by Bill Paul.
*/

#include <usbd.h>
#include <tamtypes.h>
#include <fileio.h>
#include <stdlib.h>
#include <stdio.h>
#include <kernel.h>
#include <ps2debug.h>


#include <lwip/netif.h>
#include "ps2eth.h"

typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;

#define ETHER_ADDR_LEN 6

#define K_IDVENDOR 0x846
#define K_IDPRODUCT 0x1001

#define BUFLEN 1000

#include "if_kuereg.h"
#include "kue_fw.h"

static int global;  // A very bad variable name for the DeviceID

static int cepid;   // an equally bad name for the Control End Point.

char   buffer[ BUFLEN ];  // Probably not used. 

extern  struct ethernetif klsi_etherif;


/*
 * These functions are for debuging.  It builds a full UDP packet
 * and sends the message without any tcpip stack required.
 * 
 * The MAC addresses and IP addresses need to be updated to suite
 * your own environment.
 */ 
  

struct full_udp
{
   u8 usb_len1;
   u8 usb_len2;
   u8 eth_dst[6];
   u8 eth_src[6];
   u16 eth_type;    // 0x0800
   u8 ip_ver_hlen;   // 0x45  ( 0x4 is ver 4, 0x05 is len 20 )
   u8 ip_services;   // 0x00
   u16 ip_length;    // 0x0042 is 66. 46data + 20 header.
   u16 ip_id;        // 0xcb5b
   u8 ip_flags;      // 0x00
   u8 ip_frag_offset; // 0x0000 
   u8 ip_ttl;          // 0x80   ttl = 128.
   u8 ip_protocol;     // 0x11   UDP.
   u16 ip_chksum;      // 0xedfb  header checksum.
   struct ip_addr ip_src;
   struct ip_addr ip_dst;
   u16 udp_src_port;   // 0x0456 (1110)
   u16 udp_dst_port;   // 0x0035 (53)
   u16 udp_len;        // 0x002e (46) 38data + 8 header.
   u16 udp_chksum;     // 0x7c03
   u8  data; 
} __attribute((packed));

static u8 udp_buffer[1024];
static u8 eth_dst[6] = { 0x00, 0xD0, 0x59, 0xB5, 0xF0, 0x84 };
static u8 eth_src[6] = { 0x00, 0x02, 0xe3, 0x01, 0x82, 0x72 };

u32 chksum( char * data, u16 len )
{
  u32 acc = 0;
  u16 *ptr = (u16 *) data;

  for ( ; len > 1; len-=2 )
  {
     acc += *ptr;
     ptr++;
  }
  if ( len == 1 )
  {
     acc += htons((u16)((*(u8 *)ptr) & 0xffU) << 8 );
  }
  return acc;
}

static u8 udp_log_init = 0;

void set_udp_log( u8 level )
{
  udp_log_init = level;
}

void udp_log( char * log )
{
  struct full_udp * hdrs;
  u16 log_len = strlen( log );
  u32 chk_sum = 0;

  u16 pkt_len = log_len + 20 + 8;

  if ( udp_log_init == 0 )
     return;

  hdrs = &(udp_buffer[0]);
  hdrs->usb_len1 = (u8) (pkt_len+14);
  hdrs->usb_len2 = (u8) ((pkt_len+14) >> 8 );

  hdrs->eth_dst[0] = eth_dst[0];
  hdrs->eth_dst[1] = eth_dst[1];
  hdrs->eth_dst[2] = eth_dst[2];
  hdrs->eth_dst[3] = eth_dst[3];
  hdrs->eth_dst[4] = eth_dst[4];
  hdrs->eth_dst[5] = eth_dst[5];
  
  hdrs->eth_src[0] = eth_src[0];
  hdrs->eth_src[1] = eth_src[1];
  hdrs->eth_src[2] = eth_src[2];
  hdrs->eth_src[3] = eth_src[3];
  hdrs->eth_src[4] = eth_src[4];
  hdrs->eth_src[5] = eth_src[5];

  hdrs->eth_type = htons( 0x800 ); // 0x0800

  hdrs->ip_ver_hlen = 0x45;
  hdrs->ip_services = 0x00;
  hdrs->ip_length =  htons( pkt_len );
  hdrs->ip_id = 0x0000;
  hdrs->ip_flags = 0x00;
  hdrs->ip_frag_offset = 0x0000;
  hdrs->ip_ttl = 0x80;
  hdrs->ip_protocol = 0x11;
  hdrs->ip_chksum = 0x0000; // htons( 0xb966 );  // no checksum.
  IP4_ADDR( &(hdrs->ip_src), 172,16,10,254 );

  IP4_ADDR( &(hdrs->ip_dst), 172,16,10,40 );

  // compute chksum on header.
  chk_sum = chksum( &(hdrs->ip_ver_hlen) , 20 );
  while ( chk_sum >> 16 )
  {
     chk_sum = (chk_sum & 0x0000ffffUL) + ( chk_sum >> 16 );
  }
  hdrs->ip_chksum = ~(chk_sum & 0x0000ffffUL);

  hdrs->udp_src_port = htons( 0x0456 );
  hdrs->udp_dst_port = htons( 1110 );
  hdrs->udp_len = htons(log_len + 8 );
  hdrs->udp_chksum = 0x0000; // htons( 0x1fcd ); // no checksum.

  strcpy( &(hdrs->data), log );

  chk_sum = chksum( (u8 *) &(hdrs->udp_src_port), (u16) log_len+8 );
  chk_sum+= htons((htonl( hdrs->ip_dst.addr ) & 0xffff0000UL ) >> 16 );
  chk_sum+= htons((htonl( hdrs->ip_dst.addr ) & 0x0000ffffUL ));
  chk_sum+= htons((htonl( hdrs->ip_src.addr ) & 0xffff0000UL ) >> 16 );
  chk_sum+= htons((htonl( hdrs->ip_src.addr ) & 0x0000ffffUL ));
  chk_sum+= htons(17); //psuedo-header protocol;
  chk_sum+= htons(8+log_len); //psuedo-header len;
  while ( chk_sum >> 16 )
  {
    chk_sum = (chk_sum & 0x0000ffffUL) + ( chk_sum >> 16 );
  }
  hdrs->udp_chksum = ~(chk_sum & 0x0000ffffUL );

  pkt_len += 14+2;

  kue_do_transfer( klsi_etherif.hout, klsi_etherif.hsemout, udp_buffer, pkt_len);
}


/*
 * kue_callback - This function is called when a USB transfer is complete.
 *                The semaphore handle is passed in as an argument and
 *                signalled, letting the call that made the do_transfer
 *                complete.
 */

static void kue_callback( int resultCode, int bytes, void *arg)
{
  int semh = (int) arg;
  SignalSema( semh );
}

/*
 * kue_do_request - Performs a USB request.
 */ 

static int kue_do_request( int dev, UsbDeviceRequest *req, void *data)
{
   struct t_sema sem;
   int semh;

   sem.attr = 0;
   sem.option = 0;
   sem.init_count = 0;
   sem.max_count = 1;

   semh = CreateSema( &sem );
   UsbTransfer( cepid, data, req->length, req, kue_callback, semh ); 

   WaitSema( semh ); 

   DeleteSema( semh );
}

/*
 * keu_transfer_callback, kue_do_transfer - is a generic buffer transfer
 *  to usb.  transferBytes & transferResult are used to transfer info
 *  from callback function back to do_transfer function.
 */ 

static int transferBytes;
static int transferResult;

static void kue_transfer_callback( int resultCode, int bytes, void *arg)
{
  int semh = (int) arg;

  transferResult = resultCode;
  transferBytes = bytes;

  SignalSema( semh );
}

int kue_do_transfer( int epid,int semh, void * data, int len )
{
   UsbTransfer( epid, data, len, NULL, kue_transfer_callback, semh ); 
   WaitSema( semh ); 

   if ( transferBytes > 1 )
   {
      char * buffer = (char *)data;
   }
   return transferBytes;
}

/*
 * kue_setword - Set a USB configuration word
 */

static int kue_setword( u_int8_t breq, u_int16_t word )
{
   int dev = global;
   UsbDeviceRequest req;

   req.requesttype = USB_DIR_OUT | USB_TYPE_VENDOR; 
   req.request = breq;
   req.value = word;
   req.index = 0;
   req.length = 0;
   return kue_do_request( dev, &req, NULL );
}

/*
 * kue_ctl - read/write a control variable on USB 
 */

static int kue_ctl( int rw, u_int8_t breq, u_int16_t val, char *data, int len )
{
   int dev = global;
   UsbDeviceRequest req;

   if ( rw == KUE_CTL_WRITE)
       req.requesttype = USB_DIR_OUT | USB_TYPE_VENDOR;
   else
       req.requesttype = USB_DIR_IN | USB_TYPE_VENDOR;

   req.request = breq;
   req.value = val;
   req.index = 0;
   req.length = len;

   return kue_do_request( dev, &req, data );
}


u_int8_t eaddr[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

static int kue_read_mac( )
{
   int err;
   u_int8_t etherboadcastaddr[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

   err = kue_ctl( KUE_CTL_READ, KUE_CMD_GET_MAC, 0,
		(char *)&(eaddr[0]), 6 );

   printf( "eaddr: %i:%i:%i:%i:%i:%i\n" , eaddr[0], eaddr[1], eaddr[2], eaddr[3], eaddr[4], eaddr[5] );
   if ( bcmp( eaddr, etherboadcastaddr, 6 ) )
   {
      printf( "read mac ok\n" );
   }

   return 0;
}

/* 
 * kue_load_fw - The Kawasaki chipset requires that the firmware be
 *   loaded when it is first turned on.  
 */

static int kue_load_fw( )
{
   int err;
   u_int8_t eaddr[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
   u_int8_t etherboadcastaddr[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

   // Code not already loaded.. lets load..

   err = kue_ctl( KUE_CTL_WRITE, KUE_CMD_SEND_SCAN, 0,
		kue_code_seg, sizeof(kue_code_seg));
   if (err)
      return -1;  // Failed..


   err = kue_ctl( KUE_CTL_WRITE, KUE_CMD_SEND_SCAN, 0,
		kue_fix_seg, sizeof( kue_fix_seg));
   if (err)
      return -1; // Failed..


   err = kue_ctl( KUE_CTL_WRITE, KUE_CMD_SEND_SCAN, 0,
		kue_trig_seg, sizeof( kue_trig_seg));
   if (err)
      return -1; // Failed.. 

   return 0; // Succesfully loaded.
}

static void kue_setmulti( )
{

}

/*
 * kue_reset - reset the ethernet adapter.
 */

static void kue_reset( )
{
   int dev = global;
   int x;
   UsbDeviceRequest req;

   // SET CONFIGURATION 
   req.requesttype = 0;
   req.request = 9;
   req.value = 1;
   req.index = 0;
   req.length = 0;

   kue_do_request( dev, &req, NULL );

   DelayThread( 2 * 1000 * 1000 );  //Delay 2sec.

}


static struct ip_addr ipaddr, netmask, gw;
extern void klsi_init( struct netif *netif );
extern err_t tcpip_input(struct pbuf *p, struct netif *inp);

/*
 * usb_klsi_prob - PS2 USB specific function.  This is called when
 *   a device is attached.  If this driver recognises the device then
 *   it should return true.  Currently this only recognises the 
 *   NetGear EA101 Ethernet Adapter.  This driver should however work
 *   with a number of different Kawasaki chipsets.
 */ 

int usb_klsi_probe( int deviceID )
{
  int returnvalue = 0;
  UsbDeviceDescriptor  *device;

  global = deviceID;

  device = (UsbDeviceDescriptor *) 
  UsbGetDeviceStaticDescriptor( global,0,USB_DT_DEVICE );

  if ( device != NULL )
  {
     if ( device->idVendor == K_IDVENDOR && 
          device->idProduct == K_IDPRODUCT )
     {
         returnvalue = 1;
     }
  }
  return returnvalue;
}

/* 
 *  usb_klsi_attach - PS2 USB attach function.  After a successful probe
 *   this function is called.  This is the main guts of the configuration.
 *   It opens the p
 */

int usb_klsi_attach( int deviceID )
{
  UsbDeviceDescriptor    *device;
  UsbInterfaceDescriptor *id;
  UsbInterfaceDescriptor *ci;
  UsbEndpointDescriptor  *eds;
  UsbEndpointDescriptor  *ed;
  UsbEndpointDescriptor  *ce;
  UsbConfigDescriptor    *cd;
  int i;
  int x;
   

  device = (UsbDeviceDescriptor *) UsbGetDeviceStaticDescriptor( global,0,USB_DT_DEVICE );
  if ( device == NULL )
  {
     return -1;
  }

  id = (UsbInterfaceDescriptor *) UsbGetDeviceStaticDescriptor( global,device,USB_DT_INTERFACE );

  eds = (UsbEndpointDescriptor *) UsbGetDeviceStaticDescriptor( global,id,USB_DT_ENDPOINT);

  for ( i=0; i< id->bNumEndpoints; i++ )
  {
     ed = &(eds[i]);

     if ( ed == NULL )
     {
        return -1; // Attach Failed.
     }

     if ( (ed->bEndpointAddress & 0x80) == USB_DIR_IN &&
          ed->bmAttributes == USB_ENDPOINT_XFER_BULK )
     {
        klsi_etherif.epin = i;
     }
     else if ( (ed->bEndpointAddress & 0x80) == USB_DIR_OUT &&
               ed->bmAttributes == USB_ENDPOINT_XFER_BULK )
     {
        klsi_etherif.epout = i;
     }
     else if ( (ed->bEndpointAddress & 0x80) == USB_DIR_IN &&
               ed->bmAttributes == USB_ENDPOINT_XFER_INT )
     {
        klsi_etherif.epint = i;
     }
  }

  cd = (UsbConfigDescriptor *) UsbGetDeviceStaticDescriptor( global,0,USB_DT_CONFIG );
  if ( cd == NULL )
     return -1;

  ci = (UsbInterfaceDescriptor *) UsbGetDeviceStaticDescriptor( global,cd,USB_DT_INTERFACE );
  if ( ci == NULL )
     return -1;

  ce = (UsbEndpointDescriptor *) UsbGetDeviceStaticDescriptor( global,ci,USB_DT_ENDPOINT);
  if ( ce == NULL )
     return -1;

  // Seems that on the NetGear EA101 that the bcdDevice value 
  // is changed after the firmware is loaded.

  cepid = UsbOpenEndpoint( global, NULL );
 
  if (device->bcdDevice != 0x202 )
  {
     printf( "loading firmware.\n" );

     if ( kue_load_fw( ) )
        return -1;
  }

  printf( "reseting.\n" );
  kue_reset( );
 
  printf( "reading mac.\n" ); 
  kue_read_mac( );

  printf( "setting mac.\n" );
  kue_ctl( KUE_CTL_WRITE, KUE_CMD_SET_MAC, 0, eaddr, 6 );

  printf( "configuring.\n" );
  kue_setword(KUE_CMD_SET_PKT_FILTER, KUE_RXFILT_UNICAST|KUE_RXFILT_BROADCAST|KUE_RXFILT_PROMISC|KUE_RXFILT_ALLMULTI );
  kue_setword( KUE_CMD_SET_SOFS, 1 );
  kue_setword( KUE_CMD_SET_URB_SIZE, 64 );

  // need to open pipes before adding interface!
  klsi_etherif.hin = UsbOpenBulkEndpoint( global, &(eds[klsi_etherif.epin]) );

  klsi_etherif.hout = UsbOpenBulkEndpoint( global, &(eds[klsi_etherif.epout]) );

  // Everything is now setup.
  // Now configure this for tcpip.
  // Static IP configuration..  needs to be fixed.

  IP4_ADDR( &ipaddr, 172,16,10,254 );
  IP4_ADDR( &netmask, 255,255,255,0 );
  IP4_ADDR( &gw, 172,16,10,40 );
  netif_add( &ipaddr, &netmask, &gw, klsi_init, tcpip_input );


  DelayThread( 2 * 1000 * 1000 ); 

  // Openned connection we can now use UDP debugging.
  set_udp_log( 1 );
  lock_set_out( &udp_log);

  // Send a message via UDP debugging.
  lock_printf( "Hello!" );

  return 0;  // success
}

/* 
 * usb_klsi_detach - PS2 USB specific function.  Really should deal with
 *  this, however for now..  reset the machine. :)
 */

int usb_klsi_detach( int deviceID )
{
  return -1;
}


UsbDriver usb_probe_ops  __attribute__((aligned(64)));

int _start( int argc, char **argv)
{
   int ret;
   void * usb;
   int i,x;
   struct t_thread t;

   printf( "USB KLSI Driver\n" );


   usb_probe_ops.name = "klsi";
   usb_probe_ops.probe = usb_klsi_probe;
   usb_probe_ops.connect = usb_klsi_attach;
   usb_probe_ops.disconnect = usb_klsi_detach;

   ret = UsbRegisterDriver( &usb_probe_ops );
   if ( ret < 0 ) 
   { 
      printf( "RegisterLdd returned %i\n", ret );
   }

   return 0;
} 

