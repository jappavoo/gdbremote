#ifndef __GDB_REMOTE_H__
#define __GDB_REMOTE_H__

extern FILE *DLOGFILE;

#define GDR_DPRINTF(...) { if (DLOGFILE) fprintf (DLOGFILE, __VA_ARGS__); }

typedef char (*GdbRemoteGetChar)(void *state);
typedef void (*GdbRemotePutChar)(void *state, char c);


/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */

/* account for 8 bytes per register and we double the space to be sure we 
   have enough for extra communications */
#define GDBREMOTE_BUFMAX 4096

typedef uint8_t i387_ext[10] ;
typedef unsigned __int128 uint128_t;

struct segcacheentry {
  uint64_t base;
  uint32_t limit;
  uint16_t selector;
  uint8_t  type;
  uint8_t present, dpl, db, s, l, g, avl;
} __attribute((packed));

struct dtable {
  uint64_t base;
  uint16_t limit;
} __attribute((packed));

typedef struct segcacheentry  sce_t;
typedef struct dtable         dtable_t;

// fixme:  have this generated from kex.x86-64.xml
#define X_REG64_MEMBERS \
    X(uint64_t, rax)	\
    X(uint64_t, rbx)	\
    X(uint64_t, rcx)	\
    X(uint64_t, rdx)	\
    X(uint64_t, rsi)	\
    X(uint64_t, rdi)	\
    X(uint64_t, rbp)	\
    X(uint64_t, rsp)	\
    X(uint64_t, r8)	\
    X(uint64_t, r9)	\
    X(uint64_t, r10)	\
    X(uint64_t, r11)	\
    X(uint64_t, r12)	\
    X(uint64_t, r13)	\
    X(uint64_t, r14)	\
    X(uint64_t, r15)	\
    X(uint64_t, rip)	\
    X(uint32_t, eflags)	\
    X(uint32_t, cs)	\
    X(uint32_t, ss)	\
    X(uint32_t, ds)	\
    X(uint32_t, es)	\
    X(uint32_t, fs)	\
    X(uint32_t, gs)	\
    X(sce_t, csseg)	\
    X(sce_t, ssseg)	\
    X(sce_t, dsseg)	\
    X(sce_t, esseg)	\
    X(sce_t, fsseg)	\
    X(sce_t, gsseg)     \
    X(sce_t, tr)        \
    X(sce_t, ldt)       \
    X(dtable_t, gdt)	\
    X(dtable_t, idt)	\
    X(uint64_t, cr0)	\
    X(uint64_t, cr2)	\
    X(uint64_t, cr3)	\
    X(uint64_t, cr4)	\
    X(uint64_t, cr8)	\
    X(uint64_t, efer)   \
    X(i387_ext, st0)	\
    X(i387_ext, st1)	\
    X(i387_ext, st2)	\
    X(i387_ext, st3)	\
    X(i387_ext, st4)	\
    X(i387_ext, st5)	\
    X(i387_ext, st6)	\
    X(i387_ext, st7)    \
    X(uint32_t, fctrl)	\
    X(uint32_t, fstat)	\
    X(uint32_t, ftag)	\
    X(uint32_t, fiseg)	\
    X(uint32_t, fioff)	\
    X(uint32_t, foseg)	\
    X(uint32_t, fooff)	\
    X(uint32_t, fop)	\
    X(uint128_t, xmm0)	\
    X(uint128_t, xmm1)	\
    X(uint128_t, xmm2)	\
    X(uint128_t, xmm3)	\
    X(uint128_t, xmm4)	\
    X(uint128_t, xmm5)	\
    X(uint128_t, xmm6)	\
    X(uint128_t, xmm7)  \
    X(uint128_t, xmm8)	\
    X(uint128_t, xmm9)	\
    X(uint128_t, xmm10)	\
    X(uint128_t, xmm11)	\
    X(uint128_t, xmm12)	\
    X(uint128_t, xmm13)	\
    X(uint128_t, xmm14)	\
    X(uint128_t, xmm15) \
    X(uint32_t, mxcsr)
  
struct Reg64State {
#define X(type, member) type member;
  X_REG64_MEMBERS
#undef X  
}  __attribute__((packed));

#define X(_,__) +1
static const unsigned int REG64_NUMREG = X_REG64_MEMBERS;
#undef X

// WARNING: if these change these at all you must update Reg32Map
// fixme:  have this generated from kex.i386.xml
#define X_REG32_MEMBERS \
    X(uint32_t, eax)	\
    X(uint32_t, ecx)	\
    X(uint32_t, edx)	\
    X(uint32_t, ebx)	\
    X(uint32_t, esp)	\
    X(uint32_t, ebp)	\
    X(uint32_t, esi)	\
    X(uint32_t, edi)	\
    X(uint32_t, eip)	\
    X(uint32_t, eflags)	\
    X(uint32_t, cs)	\
    X(uint32_t, ss)	\
    X(uint32_t, ds)	\
    X(uint32_t, es)	\
    X(uint32_t, fs)	\
    X(uint32_t, gs)	\
    X(sce_t, csseg)	\
    X(sce_t, ssseg)	\
    X(sce_t, dsseg)	\
    X(sce_t, esseg)	\
    X(sce_t, fsseg)	\
    X(sce_t, gsseg)     \
    X(sce_t, tr)        \
    X(sce_t, ldt)       \
    X(dtable_t, gdt)	\
    X(dtable_t, idt)	\
    X(uint32_t, cr0)	\
    X(uint32_t, cr2)	\
    X(uint32_t, cr3)	\
    X(uint32_t, cr4)	\
    X(uint32_t, cr8)	\
    X(uint64_t, efer)   \
    X(i387_ext, st0)	\
    X(i387_ext, st1)	\
    X(i387_ext, st2)	\
    X(i387_ext, st3)	\
    X(i387_ext, st4)	\
    X(i387_ext, st5)	\
    X(i387_ext, st6)	\
    X(i387_ext, st7)    \
    X(uint32_t, fctrl)	\
    X(uint32_t, fstat)	\
    X(uint32_t, ftag)	\
    X(uint32_t, fiseg)	\
    X(uint32_t, fioff)	\
    X(uint32_t, foseg)	\
    X(uint32_t, fooff)	\
    X(uint32_t, fop)	\
    X(uint128_t, xmm0)	\
    X(uint128_t, xmm1)	\
    X(uint128_t, xmm2)	\
    X(uint128_t, xmm3)	\
    X(uint128_t, xmm4)	\
    X(uint128_t, xmm5)	\
    X(uint128_t, xmm6)	\
    X(uint128_t, xmm7)  \
    X(uint32_t, mxcsr)
  
struct Reg32State {
#define X(type, member) type member;
  X_REG32_MEMBERS
#undef X  
}  __attribute__((packed));

#define X(_,__) +1
static const unsigned int REG32_NUMREG = X_REG32_MEMBERS;
#undef X

union GdbRegState {
  struct Reg64State reg64State;
  struct Reg32State reg32State; 
}__attribute__((packed)) ;

struct GdbRemoteDesc {
  char inBuffer[GDBREMOTE_BUFMAX];
  char outBuffer[GDBREMOTE_BUFMAX];
  union GdbRegState regState;
  void *state;
  char *lastPkt;
  GdbRemotePutChar putChar;
  GdbRemoteGetChar getChar;
  int gdbSignal;
  int interruptedIO;
  int writtenIO;
  int readIO;
};

extern  void GdbRemoteInit(struct GdbRemoteDesc *grd,
			   char *dlogPath,
			   void *state,
			   GdbRemoteGetChar gcf,
			   GdbRemotePutChar pcf,
			   int startSingleStep,
			   int longMode
		   );

extern  int  GdbOpen(struct GdbRemoteDesc *grd,
		     const char *pathname, int flags, int mode);
extern  int  GdbClose(struct GdbRemoteDesc *grd, int fd);

extern  int  GdbRead(struct GdbRemoteDesc *grd, 
              int fd, void *buf, unsigned int count, 
              int targetMem);
extern  int  GdbWrite(struct GdbRemoteDesc *grd,
		      int fd, const void *buf, unsigned int count,
		      int targetMem);
extern  int  GdbWriteConsole(struct GdbRemoteDesc *grd,
			     const void *buf, int count,
			     int targetMem);
extern  int  GdbReadConsole(struct GdbRemoteDesc *grd,
                 void *buf, int count,
                 int targetMem);
extern  void GdbRemoteLoop(struct GdbRemoteDesc *grd);
extern  void GdbRemoteSignalTrap(struct GdbRemoteDesc *grd);
extern  void GdbRemoteSignalIO(struct GdbRemoteDesc *grd);
extern  void GdbRemoteSignalAbort(struct GdbRemoteDesc *grd);
extern  void GdbRemoteSignalInt(struct GdbRemoteDesc *grd);

#endif
