// [XCDE] T-LINUX: stackful host fibers using glibc ucontext on Linux x86-64.
#pragma once
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <ucontext.h>
#define CALLBACK
#define SIZE_T size_t
#define GetLastError() ((unsigned long)errno)
typedef struct LinuxFiber {
    ucontext_t context;
    void *stack;
    void (*entry)(void *);
    void *arg;
} LinuxFiber;
static __thread LinuxFiber root_context;
static __thread LinuxFiber *active_context;
static int IsThreadAFiber(void) { return active_context != NULL; }
static void *GetCurrentFiber(void) { return active_context; }
static void *ConvertThreadToFiber(void *unused) {
    (void)unused;
    active_context = &root_context;
    return active_context;
}
static void linux_fiber_entry(unsigned lo, unsigned hi) {
    LinuxFiber *f = (LinuxFiber *)((uintptr_t)lo | ((uintptr_t)hi << 32));
    f->entry(f->arg);
    abort();
}
static void *CreateFiberEx(size_t commit, size_t reserve, unsigned flags, void (*entry)(void *), void *arg) {
    (void)commit; (void)flags;
    LinuxFiber *f = calloc(1, sizeof *f);
    if (!f) return NULL;
    f->stack = malloc(reserve);
    if (!f->stack || getcontext(&f->context)) { free(f->stack); free(f); return NULL; }
    f->context.uc_stack.ss_sp = f->stack;
    f->context.uc_stack.ss_size = reserve;
    f->entry = entry; f->arg = arg;
    uintptr_t ptr = (uintptr_t)f;
    makecontext(&f->context, (void (*)(void))linux_fiber_entry, 2, (unsigned)ptr, (unsigned)(ptr >> 32));
    return f;
}
static void DeleteFiber(void *ptr) {
    LinuxFiber *f = ptr;
    free(f->stack); free(f);
}
static void SwitchToFiber(void *ptr) {
    LinuxFiber *from = active_context, *to = ptr;
    active_context = to;
    if (swapcontext(&from->context, &to->context)) abort();
}
