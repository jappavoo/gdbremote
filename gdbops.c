#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "gdbremote.h"

#define NYI assert(0);

void
gdbSingleStepOn(void * ptr)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
}


void
gdbSingleStepOff(void * ptr)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
}

void
gdbBreakPointsOn(void * ptr)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
}

void
gdbSetRegisters(void * ptr, union GdbRegState *reg)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
}

int
gdbGetMem(void * ptr, uint64_t addr, int bytes, char **memData)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
  return bytes;
}

int
gdbGetRegisters(void * ptr, union GdbRegState *reg)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
  return 1;
}

int
gdbIsLongMode(void * ptr)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
  return 1;
}

void
gdbKill(void * ptr)
{
  GDR_DPRINTF("%s: %p\n", __func__, ptr);
}


