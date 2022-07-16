#define _GNU_SOURCE
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <inttypes.h>
#include <sched.h>

#include "gdbremote.h"

// put gdb remote code into debug mode ... will put debug info
// into DLOG_FILE
#define DEBUG

#ifdef DEBUG
#define DLOG_FILE "gdbremote.dlog"
#else
#define DLOG_FILE NULL
#endif

// example of how to pin this process to a cpu
// not needed but figured it is worth keeping
// in template of use
#define PIN_CPU

#ifdef PIN_CPU
#define CPU_2_PIN_TOO -1
#endif

struct GdbRemoteDesc grd;


static void
gdbPutChar(void *ptr, char c)
{
  // FIXME:  JA takecare of detecting errors like gdb connection lost here
  if (write(STDOUT_FILENO, &c, 1)!=1) err(1, "stdout closed");
}

static char
gdbGetChar(void *ptr)
{
  char c;

  // FIXME:  JA takecare of detecting errors like gdb connection lost here
 again:
  if (read(STDIN_FILENO, &c, 1)<=0) {
    if (errno == EINTR || errno == EAGAIN) goto again;
    err(1, "stdin closed");
  }
  return c;
}

struct GdbRemoteDesc grd;  

int pinCpu(int);

int main(int argc, char **argv)
{
  
  // for good measure (not really necessary given that it is in bss)
  bzero(&grd, sizeof(grd));


#ifdef PIN_CPU
  int cpu;  
  cpu = pinCpu(CPU_2_PIN_TOO);
#endif

  
  GdbRemoteInit(&grd, // Gdb remote protocol state
		DLOG_FILE,
		NULL,    // pointer that will be passed back to all our gdbops
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

  GDR_DPRINTF("PINNED TO CPU: %d\n", cpu);
}
