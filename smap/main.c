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

#include <lwip/netif.h>
#include "smap.h"

#define PS2SPD_PIO_DIR  0xb000002c
#define PS2SPD_PIO_DATA 0xb000002e

struct _ip_info
{
	struct ip_addr ipaddr;
	struct ip_addr netmask;
	struct ip_addr gw;
} ip_info;

void ResetPollThread();

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

#define	SMAP16(offset)	\
	(*(volatile u_int16_t *)(SMAP_BASE + (offset)))


//////////////////////////////////////////////////////////////////////////
// Some global stuff
int irqCounter;
static int smapEvent;

unsigned int unhandledIrq = 0;

static int smapEnabled = 1;
static int semEnabledVar;

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

    // XXX: Needed?
    {
        int oldstat;
        CpuDisableIntr();
        DisableIntr(0xD, &oldstat);
        CpuEnableIntr();
//        printf("DEV9: oldstat %x\n", oldstat);
    }

//    printf("Enabling hw access!\n");

    UNKN_1020 |= 0x2000;
    UNKN_1420 = 0x51011;
    UNKN_1418 = 0xe01a3043;
    UNKN_141C = 0xef1a3043;

    temp16 = UNKN_146C;

    if (temp16 == 0)
    {
        /* Need to map hw etc..? */
//        printf("'Starting' dev9\n");

        UNKN_1466 = 1;
        UNKN_146C = 0;
        UNKN_1464 = UNKN_146C;
        UNKN_1460 = UNKN_1464;

        if (UNKN_1462 & 0x01) {
//            printf("hohum, some error?\n");
            return -2;
        }
        else
        {
            UNKN_146C = 0;
            UNKN_146C |= 4;
            DelayThread(500*1000);
            
            UNKN_1460 |= 1;
            UNKN_146C |= 1;
            DelayThread(500*1000);
            
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
static void
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
static void
disableDev9(void)
{
	UNKN_1466 = 1;
	UNKN_146C = 0;
	UNKN_1464 = UNKN_146C;
	UNKN_1460 = UNKN_1464;
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

//    printf("smap: proc running\n");

	printf("SMAP: Initialising hardware...\n");

    detectAndInitDev9();
//    printf("smap: detect & init hw done\n");

    smap_init();
//    printf("smap: initialized, starting...\n");

    enableDev9Intr();
//    printf("smap: dev9 interrupt enabled...\n");

    // Moved to low_level_init() in smapif.c
    //    smap_start();

    memcpy(&ipaddr, &ip_info.ipaddr, sizeof(struct ip_addr));
    memcpy(&netmask, &ip_info.netmask, sizeof(struct ip_addr));
    memcpy(&gw, &ip_info.gw, sizeof(struct ip_addr));

	netif_add(&ipaddr, &netmask, &gw, smapif_init, tcpip_input);

    // XXX: Needed?
    DelayThread( 2 * 1000 * 1000 ); 

    // Return from irx init (i.e. we are ready to handle requests)
    SignalSema((int)arg);

    // Interrupt loop
    do {

		WaitSema(semEnabledVar);
		if(!smapEnabled) break;
		SignalSema(semEnabledVar);

        WaitEventFlag(smapEvent, 0xffff, 0x11, &eventStat);
        if (!(eventStat & 0x1)) {
            // This is not an event from the irq handler.. :P
//            printf("unknown event set\n");
            continue;
        }

        irq = smap_interrupt();

        CpuSuspendIntr(&oldIntr);
        smap_intr_interrupt_XXable(1);
        CpuResumeIntr(oldIntr);

        aliveCounter++;

    } while (1);


    printf("Closing smap\n");

    smap_stop();

	printf("Disabling dev9\n");

	disableDev9();

    printf("Smap thread done!\n");

    ExitThread();
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

   if(argc < 2)
   {
		printf("SMAP: No ip_info structure passed to module!\n");
		return 1;
   }

   memcpy(&ip_info, argv[1], sizeof(struct _ip_info));

   sem_info.attr = 1;
   sem_info.option = 1;
   sem_info.init_count = 0;
   sem_info.max_count = 1;

   sem = CreateSema(&sem_info);
   if (sem <= 0) {
       printf( "SMAP: CreateSema failed %i\n", sem);
       return 1;
   }

   sem_info.attr = 1;
   sem_info.option = 0;
   sem_info.init_count = 1;
   sem_info.max_count = 1;

   semEnabledVar = CreateSema(&sem_info);
   if (semEnabledVar <= 0) {
       printf( "SMAP: CreateSema failed %i\n", semEnabledVar);
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
   mythread.priority = 0x15;

   pid = CreateThread(&mythread);

   if (pid > 0) {
       if ((i=StartThread(pid, (void *)sem)) < 0) {
           printf("StartThread failed (%d)\n", i);
       }
   } 
   else {
       printf("CreateThread failed (%d)\n", pid);
   }
   
   WaitSema(sem);
   DeleteSema(sem);

   // Start reset poll thread
   mythread.type = 0x02000000;
   mythread.unknown = 0;
   mythread.function = ResetPollThread;
   mythread.stackSize = 0x800;
   mythread.priority = 0x20;

   pid = CreateThread(&mythread);

   if (pid > 0) {
       if ((i=StartThread(pid, NULL)) < 0) {
           printf("StartThread failed (%d)\n", i);
       }
   } 
   else {
       printf("CreateThread failed (%d)\n", pid);
   }

   printf("SMAP: Init done!\n");
   return 0;
}


//////////////////////////////////////////////////////////////////////////
// Experimental shutdown stuff...

#define CDVD_REG_X	*((volatile unsigned int*)0xbf402008)

void smapEnd()
{
	WaitSema(semEnabledVar);
	smapEnabled = 0;
	SignalSema(semEnabledVar);

	SetEventFlag(smapEvent, 1);
}

void ResetPollThread()
{
	while(1)
	{
		if(CDVD_REG_X & 0x04)
		{
			smapEnd();
			return;
		}

		DelayThread(1 * 1000);
	}
}
