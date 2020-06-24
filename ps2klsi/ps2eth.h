

#include <tamtypes.h>
#include <thsemap.h>

struct ethernetif
{
  struct eth_addr * ethaddr;
  u8    epin;    // End Point of bulk in.
  u8    epout;   // End Point of bulk out.
  u8    epint;   // End Point of interrupt.  Not used.
  int   hcontrol;  // handle to control endpoint 0.
  int   hin;       // handle to bulk in.
  int   hout;      // handle to bulk out.
  iop_sema_t semin;  // input wait semaphore.
  int   hsemin;		// handle to input wait semaphore.
  iop_sema_t semout;  // output wait semaphore.
  int   hsemout;         // handle to output wait semaphore.
};
