/*
 * Copyright (c) Tord Lindstrom (pukko@home.se)
 */

#include <tamtypes.h>
#include <fileio.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysclib.h>
#include <kernel.h>
#include <thevent.h>
#include <thsemap.h>
#include <vblank.h>
#include <timer.h>

#include <lwip/netif.h>
#include "smap.h"

//#define DEBUG



#define PS2SPD_PIO_DIR  0xb000002c
#define PS2SPD_PIO_DATA 0xb000002e

//////////////////////////////////////////////////////////////////////////

#define UNKN_1020   *(volatile unsigned int *)0xbf801020
#define UNKN_1418   *(volatile unsigned int *)0xbf801418
#define UNKN_141C   *(volatile unsigned int *)0xbf80141c
#define UNKN_1420   *(volatile unsigned int *)0xbf801420

#define UNKN_1460   *(volatile unsigned short *)0xbf801460
#define UNKN_1462   *(volatile unsigned short *)0xbf801462
#define UNKN_1464   *(volatile unsigned short *)0xbf801464
#define UNKN_1466   *(volatile unsigned short *)0xbf801466
#define UNKN_146C   *(volatile unsigned short *)0xbf80146c
#define UNKN_146E   *(volatile unsigned short *)0xbf80146e

#define UNKN_POFF   *(volatile unsigned char *)0xbf402008

#define	SMAP16(offset)	\
	(*(volatile u_int16_t *)(SMAP_BASE + (offset)))


//////////////////////////////////////////////////////////////////////////
// Some global stuff
int irqCounter;
static int smapEvent;

unsigned int unhandledIrq = 0;

int ArpMutex;

//////////////////////////////////////////////////////////////////////////
static int 
smapIrq(void *data)
{
    unsigned short smapIntr;


    if(SMAP16(SMAP_INTR_ENABLE) & SMAP16(SMAP_INTR_STAT)) {
        irqCounter++;

        do {
            smapIntr = SMAP16(SMAP_INTR_STAT);
            if (!(smapIntr & INTR_BITMSK)) {
                // Uh.. Strange
                unhandledIrq = (~INTR_BITMSK) & smapIntr;
                break;
            }
            else {
                smap_intr_interrupt_XXable(0);
                iSetEventFlag(smapEvent, 1);
            }
        } while (SMAP16(SMAP_INTR_STAT) & SMAP16(SMAP_INTR_ENABLE));
    }

    UNKN_1466 = 1;
    UNKN_1466 = 0;

    return 1;
}

//////////////////////////////////////////////////////////////////////////

void
disableDev9(void)
{
    // This will turn off dev9..
    UNKN_1466 = 1;
    UNKN_146C = 0;
    UNKN_1464 = UNKN_146C;
    UNKN_1460 = UNKN_1464;

    UNKN_POFF |= 0x4;

}

//////////////////////////////////////////////////////////////////////////
static int
detectAndInitDev9(void)
{

    unsigned short temp16;
    unsigned int oldint;

    /* 0xF0 & *(0xbf80146e) == 0x30 -> What dev9 device we're dealing with */
    temp16 = UNKN_146E;
    temp16 &= 0xf0;
    if (temp16 == 0x20) {
        printf("CXD9566 detected, but no driver.. sorry\n");
        return 1;
    }

    if (temp16 != 0x30) {
        printf("Unknown dev9 device detected\n");
        return -1;
    }

    dbgprintf("dev9: Enabling hw access!\n");

    UNKN_1020 |= 0x2000;
    UNKN_1420 = 0x51011;
    UNKN_1418 = 0xe01a3043;
    UNKN_141C = 0xef1a3043;

    temp16 = UNKN_146C;

    if (temp16 == 0)
    {
        /* Need to map hw etc..? */

        UNKN_1466 = 1;
        UNKN_146C = 0;
        UNKN_1464 = UNKN_146C;
        UNKN_1460 = UNKN_1464;

        if (UNKN_1462 & 0x01) {
            printf("hohum, some error?\n");
            return -2;
        }
        else
        {
            UNKN_146C = 0;
            UNKN_146C |= 4;
            DelayThread(200*1000);
            
            UNKN_1460 |= 1;
            UNKN_146C |= 1;
            DelayThread(200*1000);
            
            temp16 = SMAP16(0x02); // Wonder why?
            //            temp16 = *(volatile unsigned short *)0xb0000002; // XXX: What's this reg name?
        }        

    }


    {
        struct t_event event;

        event.attr = 0;
        event.option = 0;
        event.init_mask = 0;
        smapEvent = CreateEventFlag(&event);
    }

    if (smapEvent <= 0)
    {
        printf("Could not create event flag (%x)\n", smapEvent);
        return -3;
    }

    CpuSuspendIntr(&oldint);
    temp16 = SMAP16(SMAP_INTR_ENABLE);
    SMAP16(SMAP_INTR_ENABLE) = 0;
    temp16 = SMAP16(SMAP_INTR_ENABLE);
    CpuResumeIntr(oldint);

    UNKN_1464 = 3;

    return 0;
}

//////////////////////////////////////////////////////////////////////////
void
enableDev9Intr(void)
{

    // Enable interrupt handler
    CpuDisableIntr();

    RegisterIntrHandler(0xD, 1, smapIrq, (void *)smapEvent);
    EnableIntr(0xD);

    CpuEnableIntr();

    UNKN_1466 = 0;
}

//////////////////////////////////////////////////////////////////////////

#define RESET_POLL_INTERVAL 100*1000
static struct timestamp clock_ticks;

//////////////////////////////////////////////////////////////////////////

static unsigned int 
powerOffHandler(void *arg)
{

    if (UNKN_POFF & 0x4) {
        iSetEventFlag(smapEvent, 4);
        return 0;
    }

    return (unsigned int)arg;
}

//////////////////////////////////////////////////////////////////////////
void
installPowerOffHandler(void)
{
    USec2SysClock(RESET_POLL_INTERVAL, &clock_ticks);
    SetAlarm(&clock_ticks, powerOffHandler, (void *)clock_ticks.clk);
}

//////////////////////////////////////////////////////////////////////////
static struct ip_addr ipaddr, netmask, gw;
extern err_t tcpip_input(struct pbuf *p, struct netif *inp);
extern void smapif_init(struct netif *netif);

//////////////////////////////////////////////////////////////////////////
void
smapthread(void *arg)
{
    int oldIntr;
    int eventStat;
    unsigned int irq;
    int aliveCounter = 0;
    struct netif *smapif;

    dbgprintf("smap: proc running\n");

    detectAndInitDev9();
    dbgprintf("smap: detect & init hw done\n");

    smap_init();
    dbgprintf("smap: initialized, starting...\n");

    installPowerOffHandler();
    // Moved to low_level_init() in smapif.c
    //    enableDev9Intr();
    //    printf("smap: dev9 interrupt enabled...\n");
    //    smap_start();

    // IP address stuff is moved to _start()
    //    IP4_ADDR(&ipaddr, 192,168,0,10 );
    //    IP4_ADDR(&netmask, 255,255,255,0 );
    //    IP4_ADDR(&gw, 192,168,0,1);
    smapif = netif_add(&ipaddr, &netmask, &gw, smapif_init, tcpip_input);
    netif_set_default(smapif);

    // Return from irx init (i.e. we are ready to handle requests)
    SignalSema((int)arg);

    dbgprintf("SMAP: Ready\n");
    // Interrupt loop
    do {

        WaitEventFlag(smapEvent, 0xffff, 0x11, &eventStat);
        if (!(eventStat & 0xf)) {
            // This is not an event from the irq handler.. :P
            printf("unknown event set\n");
            continue;
        }

        if (eventStat & 8) {
            // Dbg event
            printf("smap: dbg event\n");
        }

        if (eventStat & 4) {
            // PowerOff event
            break;
        }

        if (eventStat & 1) {
            // Smap irq event
            irq = smap_interrupt();

            CpuSuspendIntr(&oldIntr);
            smap_intr_interrupt_XXable(1);
            CpuResumeIntr(oldIntr);
            dbgprintf("smap irq\n");
        }

        aliveCounter++;

    } while (1);


    printf("Closing smap\n");

    smap_stop();

	disableDev9();

    dbgprintf("Smap thread done!\n");

    ExitDeleteThread();
}

//////////////////////////////////////////////////////////////////////////
// Quickly made (but hopefully working) inet_addr()
unsigned int
inet_addr(char *str)
{
    char *cp;
    char c;
    unsigned int val;
    int part;
    unsigned int address;


    cp = str;
    val = 0;
    part = 0;
    address = 0;

    while ((c = *cp) != '\0') {
        // hm, should fix ctype_table stuff (isdigit for example)
        if ((c >= '0') && (c <= '9')) {
            val = (val * 10) + (c - '0');
            cp++;
            continue;
        }
        if (*cp == '.')
        {
            if ((part >= 3) || (val > 255)) {
                // Illegal
                return -1;
            }
            address |= val << (part * 8);
            part++;
            cp++;
            val = 0;
        }
    }
    address |= val << (part * 8);
    return address ;
}

//////////////////////////////////////////////////////////////////////////
// Initalisation
int
_start( int argc, char **argv)
{
   int pid;
   int i;
   struct t_thread mythread;
   struct t_sema sem_info;
   int sem;

   FlushDcache();
   CpuEnableIntr(0);


   printf("SMAP: argc %d\n", argc);
   // Parse ip args.. all args or nuthing w/o real error checking..
   // We really should fix this to some ip= netmask= gw=
   if (argc == 4) {
       printf("SMAP: %s %s %s\n", argv[1], argv[2], argv[3]);
       ipaddr.addr = inet_addr(argv[1]);
       netmask.addr = inet_addr(argv[2]);
       gw.addr = inet_addr(argv[3]);
   }
   else
   {
       // Set some defaults
       IP4_ADDR(&ipaddr, 192,168,0,80 );
       IP4_ADDR(&netmask, 255,255,255,0 );
       IP4_ADDR(&gw, 192,168,0,1);
   }

   sem_info.attr = 1;
   sem_info.option = 1;
   sem_info.init_count = 0;
   sem_info.max_count = 1;

   sem = CreateSema(&sem_info);
   if (sem <= 0) {
       printf( "CreateSema failed %i\n", sem);
       return 1;
   }

   sem_info.attr = 1;
   sem_info.option = 0;
   sem_info.init_count = 1;
   sem_info.max_count = 1;

   ArpMutex = CreateSema(&sem_info);
   if (ArpMutex <= 0) {
       printf( "SMAP: CreateSema failed %i\n", ArpMutex);
       return 1;
   }

   // Start smap thread
   mythread.type = 0x02000000; // attr
   mythread.unknown = 0; // option
   mythread.function = smapthread; // entry
   mythread.stackSize = 0x1000;
   mythread.priority = 0x27;

   pid = CreateThread(&mythread);

   if (pid > 0) {
       if ((i=StartThread(pid, (void *)sem)) < 0) {
           printf("StartThread failed (%d)\n", i);
       }
   } 
   else {
       printf("CreateThread failed (%d)\n", pid);
   }

   // Dont return until smap has initalized
   WaitSema(sem);
   DeleteSema(sem);

   dbgprintf("Smap init done\n");
   return 0;
}

