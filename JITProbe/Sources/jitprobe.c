// See jitprobe.h. Plain C so the codegen is exactly what we wrote: no ARC,
// no Swift runtime, just bytes we execute.

#include "jitprobe.h"

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <libkern/OSCacheControl.h>

#ifndef MAP_JIT
#define MAP_JIT 0x800
#endif

#define PROBE_LEN 0x4000u /* iOS mapping granularity */
#define PROBE_MAGIC 0x1234

/* movz x0, #0x1234 ; ret */
static const uint32_t kCode[] = { 0xD2824680u, 0xD65F03C0u };

typedef int (*jit_fn_t)(void);

static int run_buffer(void *p) {
    sys_icache_invalidate(p, sizeof(kCode));
    jit_fn_t fn = (jit_fn_t)p;
    return fn() == PROBE_MAGIC ? 0 : -100;
}

// Stock iOS lets the mapping succeed and answers the first executed
// instruction with SIGBUS/SIGSEGV/SIGILL (same pattern as LinIniOS's own
// guarded_call in engine/kernel/mm.c). Catch it and report BLOCKED instead
// of dying: a crashing probe proves nothing.
static sigjmp_buf g_jmp;

static void fault_handler(int sig) {
    (void)sig;
    siglongjmp(g_jmp, 1);
}

static int guarded_run(void *p) {
    struct sigaction sa, obus, oseg, oill;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = fault_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGBUS, &sa, &obus);
    sigaction(SIGSEGV, &sa, &oseg);
    sigaction(SIGILL, &sa, &oill);

    int rc;
    if (sigsetjmp(g_jmp, 1) == 0) {
        rc = run_buffer(p);
    } else {
        rc = -101; /* mapped, faulted on execute */
    }

    sigaction(SIGBUS, &obus, NULL);
    sigaction(SIGSEGV, &oseg, NULL);
    sigaction(SIGILL, &oill, NULL);
    return rc;
}

int probe_rwx(void) {
    void *p = mmap(NULL, PROBE_LEN, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) return errno ? errno : -1;
    memcpy(p, kCode, sizeof(kCode));
    int rc = guarded_run(p);
    munmap(p, PROBE_LEN);
    return rc;
}

int probe_rw_then_rx(void) {
    void *p = mmap(NULL, PROBE_LEN, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) return errno ? errno : -1;
    memcpy(p, kCode, sizeof(kCode));
    if (mprotect(p, PROBE_LEN, PROT_READ | PROT_EXEC) != 0) {
        int e = errno ? errno : -1;
        munmap(p, PROBE_LEN);
        return e;
    }
    int rc = guarded_run(p);
    munmap(p, PROBE_LEN);
    return rc;
}

int probe_map_jit(void) {
    void *p = mmap(NULL, PROBE_LEN, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    if (p == MAP_FAILED) return errno ? errno : -1;
    memcpy(p, kCode, sizeof(kCode));
    if (mprotect(p, PROBE_LEN, PROT_READ | PROT_EXEC) != 0) {
        int e = errno ? errno : -1;
        munmap(p, PROBE_LEN);
        return e;
    }
    int rc = guarded_run(p);
    munmap(p, PROBE_LEN);
    return rc;
}

// Map-only variants: attempt the mapping (+mprotect) but never execute.
// Safe to auto-run anywhere — the worst outcome is an errno. A 0 here
// means "the kernel let the mapping happen", NOT "code ran".
int probe_rwx_noexec(void) {
    void *p = mmap(NULL, PROBE_LEN, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) return errno ? errno : -1;
    munmap(p, PROBE_LEN);
    return 0;
}

int probe_rwrx_noexec(void) {
    void *p = mmap(NULL, PROBE_LEN, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) return errno ? errno : -1;
    int rc = 0;
    if (mprotect(p, PROBE_LEN, PROT_READ | PROT_EXEC) != 0)
        rc = errno ? errno : -1;
    munmap(p, PROBE_LEN);
    return rc;
}

int probe_mapjit_noexec(void) {
    void *p = mmap(NULL, PROBE_LEN, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    if (p == MAP_FAILED) return errno ? errno : -1;
    int rc = 0;
    if (mprotect(p, PROBE_LEN, PROT_READ | PROT_EXEC) != 0)
        rc = errno ? errno : -1;
    munmap(p, PROBE_LEN);
    return rc;
}

const char *probe_describe_map(int rc) {
    static char buf[96];
    if (rc == 0) return "mapped OK (not executed)";
    if (rc > 0) {
        snprintf(buf, sizeof(buf), "refused (errno %d: %s)", rc, strerror(rc));
        return buf;
    }
    return "refused";
}

// Largest single anonymous PROT_NONE reservation the kernel grants, in
// bytes. Stock processes are capped by RAM size (~9-13 GB); a process with
// the dynamic-codesigning blessing ("jumbo" space) gets ~64 GB. Emulator
// fastmem needs the latter. Reservation only — no pages touched.
unsigned long long probe_max_va(void) {
    unsigned long long lo = 0, hi = (unsigned long long)64 * 1024 * 1024 * 1024;
    while (hi - lo > (unsigned long long)256 * 1024 * 1024) {
        uint64_t mid = lo + (hi - lo) / 2;
        mid &= ~((uint64_t)0x4000 - 1);
        if (mid == 0) break;
        void *p = mmap(NULL, (size_t)mid, PROT_NONE,
                       MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) {
            hi = mid;
        } else {
            munmap(p, (size_t)mid);
            lo = mid;
        }
        if (mid == 0) break;
    }
    return lo;
}

const char *probe_describe(int rc) {
    static char buf[96];
    if (rc == 0) return "PASS (code ran)";
    if (rc == -100) return "FAIL (wrong result)";
    if (rc == -101) return "BLOCKED (fault on execute)";
    if (rc > 0) {
        snprintf(buf, sizeof(buf), "BLOCKED (errno %d: %s)", rc, strerror(rc));
        return buf;
    }
    return "BLOCKED";
}
