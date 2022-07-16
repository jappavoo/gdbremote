#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gdbremote.h"

// example of how to pin this process to a cpu
// not needed but figured it is worth keeping
// in template of use
#define PIN_CPU

#ifdef PIN_CPU
#define CPU_2_PIN_TOO -1
#endif

struct GdbRemoteDesc grd;

#define DEBUG

#define DLOGFILE "gdbremote.dlog"
FILE *DLOG = NULL;

#ifdef DEBUG
#define DPRINT(str) {{fprintf(DLOG, str);} 
#define DPRINTF(fmt, ...) {fprintf(DLOG, fmt, __VA_ARGS__ );} 
#define DSTMT(stmt) { stmt; }
#else
#define VPRINT(fmt, ...) 
#define VERBOSE(stmt) 
#endif


static void
gdbPutChar(void *ptr, char c)
{
}

static char
gdbGetChar(void *ptr)
{
}

struct GdbRemoteDesc grd;  


int pinCpu(int);

int main(int argc, char **argv)
{
  
  // for good measure (not really necessary given that it is in bss)
  bzero(&grd, sizeof(grd));

#ifdef DEBUG
  DLOG = fopen(DLOGFILE, "w+");
  if (DLOG == NULL) err(1, "DLOG");
  setbuf(DLOG, NULL);
#endif

#ifdef PIN_CPU
  int cpu;  
  cpu = pinCpu(CPU_2_PIN_TOO);
#endif

  
  GdbRemoteInit(&grd, // Gdb remote protocol state
		DLOG,
		NULL,  // Gdb remote protocol processing code passes this
 		       // pointer back to all our functions
		(GdbRemoteGetChar) gdbGetChar,
		(GdbRemotePutChar) gdbPutChar,
		0,  // do not start in single step mode
		1 // long mode
		);

  // start with a breakpoint
  GdbRemoteSignalTrap(&grd);
  
  while (1) {
    GdbRemoteLoop(&grd);
  }
}

int pinCpu(int cpu)
{
  cpu_set_t  mask;
  CPU_ZERO(&mask);

  if (cpu == -1 ) {
    cpu = sched_getcpu();
    if (cpu == -1) {
      err(1, "could not determine current cpu");
    }
  }

  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
    err(1, "could not pin to cpu=%d",cpu);
  }

  if (GS.debug) {
    fprintf(GS.DLOG, "PINNED TO CPU: %d\n", cpu);
  }
}
