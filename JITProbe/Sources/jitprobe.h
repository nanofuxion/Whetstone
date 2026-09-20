// JITProbe: minimal "can this process execute code it wrote?" test.
//
// Three strategies, weakest restriction first:
//   probe_rwx()        mmap RWX, write function, call it.
//   probe_rw_then_rx() mmap RW, write function, mprotect RX, call it.
//   probe_map_jit()    mmap RW|MAP_JIT, write function, mprotect RX, call it.
//
// Each writes the same two-instruction ARM64 function
// (movz x0, #0x1234; ret) and calls it under a SIGBUS/SIGSEGV/SIGILL guard.
// WARNING: the guard cannot catch the kernel's codesigning kill. On stock
// iOS, executing an unsigned page answers SIGKILL — instant, uncatchable
// death. So the exec probes must only run on explicit user tap, with a
// crash marker written first (see ProbeViewController): a kill then reads
// as "JIT OFF", which is itself the result. Return value:
//   0          the generated code ran and returned 0x1234.
//   >0         an errno from mmap/mprotect (strategy blocked at map).
//   -100       code ran but returned the wrong value (shouldn't happen).
//   -101       mapped, but faulted on execute (catchable fault only).
//
// The *_noexec variants attempt the mapping (+mprotect) but never execute:
// safe to auto-run. 0 means "mapping allowed", >0 is the errno.
//
// Only call these on a background-free moment; they mmap/munmap 16 KB.

#ifndef jitprobe_h
#define jitprobe_h

int probe_rwx(void);
int probe_rw_then_rx(void);
int probe_map_jit(void);

int probe_rwx_noexec(void);
int probe_rwrx_noexec(void);
int probe_mapjit_noexec(void);

// Human-readable one-liner for a probe return value. Never NULL.
const char *probe_describe(int rc);
// Describer for the *_noexec return values. Never NULL.
const char *probe_describe_map(int rc);

// Largest single reservable anonymous region, in bytes (PROT_NONE, safe
// to auto-run). Stock ~9-13 GB, jumbo-blessed ~64 GB.
unsigned long long probe_max_va(void);

#endif /* jitprobe_h */
