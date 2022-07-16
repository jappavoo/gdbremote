#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <err.h>
#include "gdbremote.h"
#include <gdb/signals.h>
#include <gdb/fileio.h>
#include <stddef.h>

extern char _binary_gdbremote_x86_64_xml_start[];
extern char _binary_gdbremote_x86_64_xml_end[];

extern char _binary_gdbremote_i386_xml_start[];
extern char _binary_gdbremote_i386_xml_end[];

extern void gdbKill(void *state);
extern int gdbGetRegisters(void *state, union GdbRegState *reg);
extern int gdbSetRegisters(void *state, union GdbRegState *reg);
extern int gdbGetMem(void *state, uint64_t addr, int bytes, char **memData);
extern void gdbSingleStepOn(void *state);
extern void gdbSingleStepOff(void *state);
extern int gdbIsLongMode(void *state);
extern void gdbBreakPointsOn(void *state);

struct XMLDoc {
  unsigned char * data;
  unsigned long long len;
} targetXML;

#define FALSE 0
#define TRUE  1

FILE * DLOGFILE = NULL;


/* I have tried to separate out the protcol processing from the actual
   state manipulation and communication functionality.  I hope that
   this will lead to code that I will be able to reuse for other projects */

/* This code was developed based on the sparc-stub.c from the gdb
   source tree gdb/stubs/sparc-stub.c as per the recommendation of
   https://access.redhat.com/documentation/en-US/Red_Hat_Enterprise_Linux/
   4/html/Debugging_with_gdb/remote-stub.html
*/

#define sizeoffield(T,f) sizeof(((T *)0)->f)
#define RegMapEntry(i, T, n)			\
  [i] = { .num = i,				\
	   .offset = offsetof(T, n),            \
	  .len = sizeoffield(T, n),		\
	  .name = #n				\
  }

struct  RegMap {
  unsigned int  offset;
  unsigned int  len;
  char name[8];
};

static struct RegMap reg32Map[] =
  {
#define X(t, n) {						\
		 .offset = offsetof(struct Reg32State,n),	\
		 .len=sizeof(t),				\
		 .name = #n					\
   },
   X_REG32_MEMBERS
#undef X
  };

static struct RegMap reg64Map[] =
  {
   #define X(t, n) {						\
		 .offset = offsetof(struct Reg64State,n),	\
		 .len=sizeof(t),				\
		 .name = #n					\
   },
   X_REG64_MEMBERS
#undef X
  };


static const char hexchars[]="0123456789abcdef";

static int
hex (unsigned char ch)
{
  if (ch >= 'a' && ch <= 'f')
    return ch-'a'+10;
  if (ch >= '0' && ch <= '9')
    return ch-'0';
  if (ch >= 'A' && ch <= 'F')
    return ch-'A'+10;
  return -1;
}

static inline void
putChar(GdbRemotePutChar pc, void *dstate, char c)
{
  pc(dstate,c);
  GDR_DPRINTF("%c",c);
}

static inline char
getChar(GdbRemoteGetChar gc, void *dstate)
{
  char c = gc(dstate);
  GDR_DPRINTF("%c", c);
  return c;
}

static unsigned char *
getpacket (struct GdbRemoteDesc *grd)
{
  GdbRemotePutChar pc = grd->putChar;
  GdbRemoteGetChar gc = grd->getChar;
  void *dstate = grd->state;
  unsigned char *buffer = &(grd->inBuffer[0]);
  unsigned char checksum;
  unsigned char xmitcsum;
  int count;
  char ch;

  GDR_DPRINTF("\ngetpacket:\n");
  while (1)
    {
      /* wait around for the start character, ignore all other characters */
      while ((ch = getChar(gc, dstate)) != '$')
	;
     
retry:
      GDR_DPRINTF("\n< ");
      checksum = 0;
      xmitcsum = -1;
      count = 0;

      /* now, read until a # or end of buffer is found */
      while (count < GDBREMOTE_BUFMAX - 1)
	{
	  ch = getChar(gc, dstate);
          if (ch == '$')
            goto retry;
	  if (ch == '#')
	    break;
	  checksum = checksum + ch;
	  buffer[count] = ch;
	  count = count + 1;
	}
      buffer[count] = 0;

      if (ch == '#')
	{
	  ch = getChar(gc, dstate);
	  xmitcsum = hex (ch) << 4;
	  ch = getChar(gc, dstate);
	  xmitcsum += hex (ch);

	  if (checksum != xmitcsum)
	    {
	      putChar(pc, dstate, '-');	/* failed checksum */
	    }
	  else
	    {
	      putChar(pc, dstate, '+');	/* successful transfer */

	      /* if a sequence char is present, reply the sequence ID */
	      if (buffer[2] == ':')
		{
		  putChar(pc, dstate, buffer[0]);
		  putChar(pc, dstate, buffer[1]);

		  return &buffer[3];
		}

	      return &buffer[0];
	    }
	}
    }
}

/* send the packet in buffer.  */
static void
putpacket (struct GdbRemoteDesc *grd, int count)
{
  unsigned char checksum;
  unsigned char ch;
  GdbRemotePutChar pc = grd->putChar;
  GdbRemoteGetChar gc = grd->getChar;
  void *dstate = grd->state;
  unsigned char *buffer = &(grd->outBuffer[0]);
  
  /*  $<packet info>#<checksum>. */
  GDR_DPRINTF("\n> putpacket\n");
  do
    {
      putChar(pc, dstate, '$');
      checksum = 0;

      for (int i=0; i<count; i++) {
	ch = buffer[i];
	putChar(pc, dstate,ch);
	checksum += ch;
      }

      putChar(pc, dstate, '#');
      putChar(pc, dstate, hexchars[checksum >> 4]);
      putChar(pc, dstate, hexchars[checksum & 0xf]);

    }
  while (getChar(gc, dstate) != '+');
}

/* Convert the memory pointed to by mem into hex, placing result in buf. Places NULL
 * at end
 * Return number of bytes put in buf excluding NULL
 */
static int
mem2hex (unsigned char *buf, int bufLen, const unsigned char *mem,  int count)
{
  unsigned char ch;
  int bytes = 0;
  if (count+1 > bufLen) err(1, "mem2hex");

  while (count-- > 0)
    {
      ch = *mem++;
      *buf++ = hexchars[ch >> 4];
      *buf++ = hexchars[ch & 0xf];
      bytes += 2;
    }

  *buf = 0;

  return bytes;
}


/* convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written */

static char *
hex2mem (unsigned char *buf,  unsigned char *mem, int count)
{
  int i;
  unsigned char ch;

  for (i=0; i<count; i++)
    {
      ch = hex(*buf++) << 4;
      ch |= hex(*buf++);
      *mem++ = ch;
    }
  return mem;
}

// Following code taken from gdb source rsp-low.c
/* Return whether byte B needs escaping when sent as part of binary data.  */

static int
needs_escaping (unsigned char b)
{
  return b == '$' || b == '#' || b == '}' || b == '*';
}

/* See rsp-low.h.  */

int
remote_escape_output (const unsigned char *buffer, int len_units, int unit_size,
		      unsigned char *out_buf, int *out_len_units,
		      int out_maxlen_bytes)
{
  int input_unit_index, output_byte_index = 0, byte_index_in_unit;
  int number_escape_bytes_needed;

  /* Try to copy integral addressable memory units until
     (1) we run out of space or
     (2) we copied all of them.  */
  for (input_unit_index = 0;
       input_unit_index < len_units;
       input_unit_index++)
    {
      /* Find out how many escape bytes we need for this unit.  */
      number_escape_bytes_needed = 0;
      for (byte_index_in_unit = 0;
	   byte_index_in_unit < unit_size;
	   byte_index_in_unit++)
	{
	  int idx = input_unit_index * unit_size + byte_index_in_unit;
	  unsigned char b = buffer[idx];
	  if (needs_escaping (b))
	    number_escape_bytes_needed++;
	}

      /* Check if we have room to fit this escaped unit.  */
      if (output_byte_index + unit_size + number_escape_bytes_needed >
	    out_maxlen_bytes)
	  break;

      /* Copy the unit byte per byte, adding escapes.  */
      for (byte_index_in_unit = 0;
	   byte_index_in_unit < unit_size;
	   byte_index_in_unit++)
	{
	  int idx = input_unit_index * unit_size + byte_index_in_unit;
	  unsigned char b = buffer[idx];
	  if (needs_escaping (b))
	    {
	      out_buf[output_byte_index++] = '}';
	      out_buf[output_byte_index++] = b ^ 0x20;
	    }
	  else
	    out_buf[output_byte_index++] = b;
	}
    }

  *out_len_units = input_unit_index;
  return output_byte_index;
}

/* See rsp-low.h.  */

int
remote_unescape_input (const unsigned char *buffer, int len,
		       unsigned char *out_buf, int out_maxlen)
{
  int input_index, output_index;
  int escaped;

  output_index = 0;
  escaped = 0;
  for (input_index = 0; input_index < len; input_index++)
    {
      unsigned char b = buffer[input_index];

      if (output_index + 1 > out_maxlen)
	err(1, "Received too much data.");

      if (escaped)
	{
	  out_buf[output_index++] = b ^ 0x20;
	  escaped = 0;
	}
      else if (b == '}')
	escaped = 1;
      else
	out_buf[output_index++] = b;
    }

  if (escaped)
    err(1, "Unmatched escape character in response.");

  return output_index;
}

static void
dumpTargetXML()
{
  unsigned long long i;
  for (i=0; i<targetXML.len; i++) {
    GDR_DPRINTF("%c",targetXML.data[i]);
  }
}

static void
dumpReg32Map()
{
  GDR_DPRINTF("Validate the following with the output of gdb:  maint print c-tdesc\n");
  for (int i=0; i<(sizeof(reg32Map)/sizeof(struct RegMap)); i++) {
    GDR_DPRINTF("%d: %s: offset:%d len:%d (%d)\n", i,
	   reg32Map[i].name,
	   reg32Map[i].offset,
	   reg32Map[i].len,
	   reg32Map[i].len * 8);
  }
}

static void
singleStepOn(struct GdbRemoteDesc *grd)
{
    gdbSingleStepOn(grd->state);
}

static void
singleStepOff(struct GdbRemoteDesc *grd)
{
    gdbSingleStepOff(grd->state);
}

static void
breakPointsOn(struct GdbRemoteDesc *grd)
{
  gdbBreakPointsOn(grd->state);
}


static void
dumpReg64Map()
{
  GDR_DPRINTF("Validate the following with the output of gdb:  maint print c-tdesc\n");
  for (int i=0; i<(sizeof(reg64Map)/sizeof(struct RegMap)); i++) {
    GDR_DPRINTF("%d: %s: offset:%d len:%d (%d)\n", i,
	   reg64Map[i].name,
	   reg64Map[i].offset,
	   reg64Map[i].len,
	   reg64Map[i].len * 8);
  }
}

static void
GdbOpenDLog(char *path)
{
  if (path) {
    DLOGFILE = fopen(path, "w+");
    if (DLOGFILE == NULL) err(1, "DLOGFILE");
    setbuf(DLOGFILE, NULL);
  }
}

extern void
GdbRemoteInit(struct GdbRemoteDesc *grd,
	      char *dlogPath,
	      void *state,
	      GdbRemoteGetChar gcf,
	      GdbRemotePutChar pcf,
	      int startSingleStep,
	      int longMode
	      )
{

  GdbOpenDLog(dlogPath);
  grd->state = state;
  grd->getChar = gcf;
  grd->putChar = pcf;
  grd->gdbSignal = GDB_SIGNAL_0;
  grd->interruptedIO = 0;
  grd->writtenIO = 0;
  
  breakPointsOn(grd);
  if (startSingleStep) singleStepOn(grd);

  GDR_DPRINTF("reg32State: size in bytes:%lu Number of Registers:%d)\n",
	 sizeof(grd->regState.reg32State), REG32_NUMREG);
  dumpReg32Map();
  
  GDR_DPRINTF("reg64State: size in bytes:%lu Number of Registers:%d)\n",
	 sizeof(grd->regState.reg64State), REG64_NUMREG);
  dumpReg64Map();

  if (longMode) {
    targetXML.data = _binary_gdbremote_x86_64_xml_start;
    targetXML.len = _binary_gdbremote_x86_64_xml_end - _binary_gdbremote_x86_64_xml_start ;
  } else {
    targetXML.data = _binary_gdbremote_i386_xml_start;
    targetXML.len = _binary_gdbremote_i386_xml_end - _binary_gdbremote_i386_xml_start;
  }
  dumpTargetXML();
  
}

static int
hexToInt(char **ptr, uint64_t *intValue)
{
  int numChars = 0;
  int hexValue;

  *intValue = 0;

  while (**ptr)
    {
      hexValue = hex(**ptr);
      if (hexValue < 0)
	break;

      *intValue = (*intValue << 4) | hexValue;
      numChars ++;

      (*ptr)++;
    }

  return (numChars);
}


void setSignal(struct GdbRemoteDesc *grd, int signal) {
  grd->gdbSignal = signal;
}

enum PacketProcessRC { 
		UNHANDLED_F_PACKET = -1, 
		DONE_PACKETS = 0, 
		HANDLED_PACKET = 1
};

// return 0 if the packet 
static int
processPackets(struct GdbRemoteDesc *grd)
{
  int rc = HANDLED_PACKET;
  int skipPut = 0;
  void *state = grd->state;
  char *opkt = &(grd->outBuffer[0]);
  int outBytes = 0;
  char *ipktPtr = getpacket(grd);

  // reset output packet
  opkt[0] = 0;
  
  grd->lastPkt = ipktPtr;
  
  switch (*ipktPtr++) {
  case '?':
    outBytes = snprintf(&opkt[0], GDBREMOTE_BUFMAX, "S%02d", GDB_SIGNAL_TRAP);
    GDR_DPRINTF("GDB: ? : %s\n", opkt);
    break;
  case 'd':
    GDR_DPRINTF("GDB: d\n");
    break;
  case 'g': {
    GDR_DPRINTF("GDB: g : \n");
    if (gdbGetRegisters(state, &(grd->regState))) {
      GDR_DPRINTF("LONG MODE\n");
      outBytes = mem2hex(&(opkt[0]), GDBREMOTE_BUFMAX,
			 (char *)(&(grd->regState.reg64State)),
			 sizeof(grd->regState.reg64State));
    } else {
      outBytes = mem2hex(&(opkt[0]), GDBREMOTE_BUFMAX,
	      (char *)(&(grd->regState.reg32State)),
	      sizeof(grd->regState.reg32State));
    }
  }
    break;
  case 'G': {
    GDR_DPRINTF("GDB: G\n");
    union GdbRegState *reg = &(grd->regState);
    if (gdbIsLongMode(state)) {
      hex2mem(ipktPtr,
	      (char *)&(reg->reg64State),
	      sizeof(reg->reg64State));
    } else {
      hex2mem(ipktPtr,
	      (char *)&(reg->reg32State),
	      sizeof(reg->reg32State));
    }
    if (gdbSetRegisters(state, reg)) {
      outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "OK");
    } else {
      outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E03");
    }
  }
    break;
  case 'q':
    {
      GDR_DPRINTF("\nq packet: %s\n", ipktPtr);
      if (strncmp(ipktPtr, "Supported:",10)==0) {
	// Got supported query from gdb reply back that we
	// will define the target feature XML 
	outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "qXfer:features:read+");
      }
      if (strncmp(ipktPtr, "Xfer:features:read:target.xml:",30)==0) {
	// as is done in gdbserver code the arguments are like those to
	// an m packet
	uint64_t offset;
	uint64_t len;
	unsigned char * doc = targetXML.data;
	uint64_t docLen = targetXML.len;
	
	ipktPtr += 30;
	GDR_DPRINTF("\n got request for target.xml: %s", &ipktPtr[30]);
	if (hexToInt(&ipktPtr, &offset)
	  && *ipktPtr++ == ','
	    && hexToInt(&ipktPtr, &len)) {
	  GDR_DPRINTF("\n got request for target.xml: offset:%lu len:%lu docLen=%lu\n",
		 offset, len, docLen);
	  if (offset > docLen) {
	    // case 1: request past end
	    outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E01");
	  }  else {
	    if (offset + len < docLen) {
	      // case 2: request is contained with doc (does not make it
	      // or pass end
	      // response indicates there is more data is target.xml
	      opkt[0] = 'm';
	      outBytes++;
	    } else {
	      // case 3: request is too or past end -- no more data remains
	      // response indicates this read is the last one necessary to
	      // get all the data of target.xml
	      opkt[0] = 'l';
	      outBytes++;
	      len = docLen - offset;
	    }
	    remote_escape_output(&doc[offset],len,1,&opkt[outBytes],&outBytes,GDBREMOTE_BUFMAX-2);
	  }
	} else {
	  outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E00");
	}
      }		  
    }
    break;
  case 'm':   /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
    /* Try to read %x,%x.  */
    {
      uint64_t addr;
      uint64_t len;
      char *memData;
      GDR_DPRINTF("GDB: m:");
      if (hexToInt(&ipktPtr, &addr)
	         && *ipktPtr++ == ','
	         && hexToInt(&ipktPtr, &len))
	    {
        GDR_DPRINTF("%016lx: %lu\n", addr, len);
        if (gdbGetMem(state, addr, len, &memData) == len) {
          outBytes = mem2hex(&(opkt[0]), GDBREMOTE_BUFMAX, memData, len);
        } else {
          outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "Ex03");
        }
      } else 
        outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E01");
    }
    break;
  case 'M':
    {
      uint64_t addr;
      uint64_t len;
      char *memData;
      GDR_DPRINTF("GDB: M\n");
      if (hexToInt(&ipktPtr, &addr)
	         && *ipktPtr++ == ','
	         && hexToInt(&ipktPtr, &len)
	         && *ipktPtr++ == ':')	
      {
        GDR_DPRINTF("%016lx: %lu : %s\n", addr, len, ipktPtr);
        if (gdbGetMem(state, addr, len, &memData) == len) {
          hex2mem(ipktPtr, memData, len);
          outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "OK");
          break;
        } 
        outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E03");
      } else
        outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E01");
    }
    break;
  case 'P':
    {
      union GdbRegState *reg = &(grd->regState);
      uint64_t regnum;
      GDR_DPRINTF("GDB: P\n");
      if (hexToInt(&ipktPtr, &regnum)
	  && *ipktPtr++ == '=' ) {
	GDR_DPRINTF("GDB: regnum=%lu value=%s\n", regnum, ipktPtr);
	if (gdbGetRegisters(state, reg)) {
	  // long mode
	  if (regnum < REG64_NUMREG) {
	    struct RegMap *rm = &(reg64Map[regnum]);
	    unsigned int offset = rm->offset;
	    unsigned int len = rm->len;
	    char * regData = ((char *)(&(reg->reg64State))) + offset;
	    GDR_DPRINTF("GDB: rm: %u, %u, %s\n", offset, len, rm->name);
	    hex2mem(ipktPtr, regData, len);
	  } else {
	    outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E03");
	  }
	} else {
	  if (regnum < REG32_NUMREG) {
	    struct RegMap *rm = &(reg32Map[regnum]);
	    unsigned int offset = rm->offset;
	    unsigned int len = rm->len;
	    char * regData = ((char *)(&(reg->reg32State))) + offset;
	    GDR_DPRINTF("GDB: rm: %u, %u, %s\n", offset, len, rm->name);
	    hex2mem(ipktPtr, regData, len);
	  } else {
	    outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E03");
	  }
	}
	if (gdbSetRegisters(state, reg)) {
	  outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "OK");
	} else {
	  outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E02");
	}      
      } else {
	outBytes = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E01");
      }
    }
    break;
  case 'c':
    GDR_DPRINTF("GDB: c\n");
    setSignal(grd, GDB_SIGNAL_0);
    skipPut = 1;
    singleStepOff(grd);
    rc = DONE_PACKETS;
    break;
  case 'k':
    GDR_DPRINTF("GDB: k\n");
    gdbKill(state);
    skipPut = 1;
    rc = DONE_PACKETS;
    break;
  case 'r':
    GDR_DPRINTF("GDB: r\n");
    break;
  case 's':
    GDR_DPRINTF("GDB: s\n");
    setSignal(grd, GDB_SIGNAL_0);
    skipPut = 1;
    singleStepOn(grd);
    rc = DONE_PACKETS;
    break;
  default:
    GDR_DPRINTF("GDB: Unknown packet\n");
    // send empty reply
  }
  if (!skipPut) putpacket(grd,outBytes);
  return rc;
}


void
GdbRemoteSignalTrap(struct GdbRemoteDesc *grd)
{
  grd->gdbSignal = GDB_SIGNAL_TRAP;
}

void
GdbRemoteSignalAbort(struct GdbRemoteDesc *grd)
{
  grd->gdbSignal = GDB_SIGNAL_ABRT;
}


void
GdbRemoteSignalInt(struct GdbRemoteDesc *grd)
{
  grd->gdbSignal = GDB_SIGNAL_INT;
}

void
GdbRemoteSignalIO(struct GdbRemoteDesc *grd)
{
  grd->gdbSignal = GDB_SIGNAL_IO;
}


static int 
fileProtocolReadMem(struct GdbRemoteDesc *grd, char *ipkt, uint64_t faddr, const void *buf,
        int written, int targetMem, int count)
{
  void *state = grd->state;
  char *opkt = &(grd->outBuffer[0]);
  int   olen;
  /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
  /* Try to read %x,%x.  */
  uint64_t addr;
  uint64_t len;
  char *memData;
  
  GDR_DPRINTF("\nfileProtocolReadMem: faddr:%lx buf:%p written:%d targetMem:%d count:%d m:", faddr, buf, written, targetMem, count);
  if (hexToInt(&ipkt, &addr)
      && *ipkt++ == ','
      && hexToInt(&ipkt, &len)) {
    if ((addr == faddr) && (len <= count)) {
      GDR_DPRINTF("%016lx: %lu\n", addr, len);
      if (targetMem) {
  if (gdbGetMem(state, addr+written, len, &memData) == len)  {
    olen=mem2hex(&(opkt[0]), GDBREMOTE_BUFMAX, memData,  len);
  } else {
    olen=snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E03");
  }
      } else {
  olen=mem2hex(&(opkt[0]), GDBREMOTE_BUFMAX, buf+written, len);
      }
    } else {
      olen=snprintf(&(opkt[0]),  GDBREMOTE_BUFMAX, "E03");
    }
  } else {
    olen=snprintf(&(opkt[0]),  GDBREMOTE_BUFMAX, "E01");
  }
  return olen;
}


static int 
fileProtocolWriteMem(struct GdbRemoteDesc *grd, char *ipkt, uint64_t faddr, void *buf,
        int read, int targetMem, int count)
{
  void *state = grd->state;
  char *opkt = &(grd->outBuffer[0]);
  int   olen;

  uint64_t addr;
  uint64_t len;
  char *memData;

  GDR_DPRINTF("\nfileProtocolWriteMem: faddr:%lx buf:%p read:%d targetMem:%d count:%d m:", faddr, buf, read, targetMem, count);
  if (hexToInt(&ipkt, &addr)
      && *ipkt++ == ','
      && hexToInt(&ipkt, &len)
      && *ipkt++ == ':') {
    if ((addr == faddr) && (len <= count)) {
      GDR_DPRINTF("%016lx: %lu\n", addr, len);
      if (targetMem) {
        if (gdbGetMem(state, addr+read, len, &memData) == len)  {
          GDR_DPRINTF("%s:%d\n", __func__, __LINE__);
          hex2mem(ipkt, memData, len);
          olen = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "OK");
        } else {
          GDR_DPRINTF("%s:%d\n", __func__, __LINE__);
          olen = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "E03");
        }
      } else {
        GDR_DPRINTF("%s:%d\n", __func__, __LINE__);
        hex2mem(ipkt, buf+read, len);
        olen = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "OK");
      }
    } else {
      olen = snprintf(&(opkt[0]),  GDBREMOTE_BUFMAX, "E03");
    }
  } else {
    olen = snprintf(&(opkt[0]),  GDBREMOTE_BUFMAX, "E01");
  }
  return olen;
}

#if 0
int
GdbOpen(struct GdbRemoteDesc *grd, const char *pathname, int flags, int mode,
	int targetMem)
{
  int n;
  uint64_t FopenPathAddr = -1;
  
  if (targetMem) FopenPathAddr = pathname;

 retry:
  n = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "Fopen,%llx/%x,%x,%x",
	   FopenPathAddr, strlen(pathname),flags, mode);
  putpacket(grd,n);

  while (1) {
  }
  return 1;
}

int
GdbClose(struct GdbRemoteDesc *grd, int fd)
{
  return 1;
}

#endif

int GdbReadConsole(struct GdbRemoteDesc *grd, void *buf, int count,
  int targetMem) 
{
  return GdbRead(grd, 0, buf, count, targetMem);
}

int
GdbRead(struct GdbRemoteDesc *grd, int fd, void *buf, unsigned int count,
  int targetMem)
{
  int rc = 0;
  char *opkt = &(grd->outBuffer[0]); //not sure outBuffer or inBuffer
  void *state = grd->state;
  int read = 0;
  uint64_t FreadAddr = -1;
  char *ipkt;
  int n;

  if (targetMem) FreadAddr = (uint64_t) buf;

  if (grd->interruptedIO) read = grd->readIO;

  grd->interruptedIO = 0;
  grd->readIO = 0;

  retry:

  n = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "Fread,%x,%llx,%x",
         fd, (unsigned long long)FreadAddr, count);
  putpacket(grd,n);

  while (1) {
    ipkt = getpacket(grd);
    GDR_DPRINTF("\nGdbReadConsole: %s\n", ipkt);
    // reset output packet
    opkt[0] = 0;

    switch  (*ipkt++)  {
      case 'M': {
        n = fileProtocolWriteMem(grd, ipkt, FreadAddr, buf, read, targetMem, count);
        GDR_DPRINTF("%s:%d\n", __func__, __LINE__);
      }
        break;
        case 'F': {
      // Got a  response F packet from gdb - one way or another we are done
      // this attempt
      // parse out all returned values retval,errornum,ctl-c
      uint64_t rval=0;
      uint64_t errornum=0;
      int      ctlc=0;

      if (hexToInt(&ipkt, &rval)) {
	      if (*ipkt++ == ',' && hexToInt(&ipkt, &errornum)
	     && *ipkt++ == ',' && *ipkt == 'C' ) ctlc=1;

      	if (ctlc) rc = 0;  // if control c then we want to signal a trap to gdb
            
      	if (rval == -1) { // error write failed
      	  rc = 0;
      	  if (errornum == FILEIO_EINTR) {
      	    // file operation not done mark as interrupted
      	    grd->interruptedIO = 1;
      	    grd->readIO = read;
      	    // this indicates that io should be reissued
      	    // rather than continuing target execution
      	  }
      	} else if (rval == count) {
      	  rc = 1; // all done successfull write
      	} else {  // not an error but fewer bytes written than requested
      	  count -= rval;
      	  read += rval;
      	  goto retry;
      	}
      }
      goto done;
    }
      // case 'F': {
      //   GDR_DPRINTF("%s:%d\n", __func__, __LINE__);
      // }
        break;
    }
    // send packet as reply maybe empty if something went wrong
    putpacket(grd,n);
  }
  done:

  return rc;
}

int GdbWriteConsole(struct GdbRemoteDesc *grd, const void *buf, int count,
  int targetMem) 
{
  return GdbWrite(grd, 1, buf, count, targetMem);
}
  
  
int 
GdbWrite(struct GdbRemoteDesc *grd, int fd, const void *buf, unsigned int count,
   int targetMem)
{
  int rc = 0; // default is error
  char *opkt = &(grd->outBuffer[0]);
  void *state = grd->state;
  int written = 0;
  uint64_t FwriteAddr = -1;
  char *ipkt;
  int n;
  
  if (targetMem) FwriteAddr = (uint64_t)buf;

  if (grd->interruptedIO)  written = grd->writtenIO;

  grd->interruptedIO = 0;  // clear interrupted IO state
  grd->writtenIO = 0;

 retry:
  n = snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "Fwrite,%x,%llx,%x",
         fd, (unsigned long long)FwriteAddr, count);
  putpacket(grd,n);

  // process received packets until we get and F packet which is
  // the reply that ends our F request  -- everything is syncronous
  //    No nesting or multiplexing of request reply transactions
  while (1) {
    ipkt = getpacket(grd);
    GDR_DPRINTF("GdbWriteConsole: %s", ipkt);
    // reset output packet
    opkt[0] = 0;
  
    switch  (*ipkt++)  {
    case 'm': {
     n = fileProtocolReadMem(grd, ipkt, FwriteAddr, buf, written, targetMem, count);
        GDR_DPRINTF("%s:%d\n", __func__, __LINE__);
  }
  break;
    case 'F': {
      // Got a  response F packet from gdb - one way or another we are done
      // this attempt
      // parse out all retu;rned values retval,errornum,ctl-c
      uint64_t rval=0;
      uint64_t errornum=0;
      int      ctlc=0;

      if (hexToInt(&ipkt, &rval)) {
  if (*ipkt++ == ',' && hexToInt(&ipkt, &errornum)
    && *ipkt++ == ',' && *ipkt == 'C' ) ctlc=1;

  if (ctlc) rc = 0;  // if control c then we want to signal a trap to gdb
      
  if (rval == -1) { // error write failed
    rc = 0;
    if (errornum == FILEIO_EINTR) {
      // file operation not done mark as interrupted
      grd->interruptedIO = 1;
      grd->writtenIO = written;
      // this indicates that io should be reissued
      // rather than continuing target execution
    }
  } else if (rval == count) {
    rc = 1; // all done successfull write
  } else {  // not an error but fewer bytes written than requested
    count -= rval;
    written += rval;
    goto retry;
  }
      }
      goto done;
    }
      break;
    }
    // send packet as reply maybe empty if something went wrong
      putpacket(grd,n);
  }
  done:
  
  return rc;
}

#if 0
  int GdbWriteConsole(struct GdbRemoteDesc *grd, const void *buf, int count,
		    int targetMem) {
    return 0;
  }
#endif

void
GdbRemoteLoop(struct GdbRemoteDesc *grd)
{
  // send gdb a packet updating it with out state

  if (grd->gdbSignal != GDB_SIGNAL_0) {
    char *opkt = &(grd->outBuffer[0]);
    int n=snprintf(&(opkt[0]), GDBREMOTE_BUFMAX, "S%02d", grd->gdbSignal);
    putpacket(grd,n);
    // now give control over to gdb by processing packets
    // until one of the packets tells us to hand
    // control back to the execution engine
    while (processPackets(grd));
  }
}

