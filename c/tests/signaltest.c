/* signaltest.c : test code for signal handling

Called from outside:
  signal_run_all_tests() - runs all signal handling tests

*/

#include <xerostest.h>
#include <xeroskernel.h>

static void signaltest_basic_signal(void);
static void print_signal(void* cntx);
static void testfunc(void);

/**
 * Runs all signal tests.
 */
void signal_run_all_tests(void) {
    signaltest_basic_signal();
    DEBUG("Done all signal tests. Looping forever\n");
    while(1);
}

/**
 * Tests that we can send a signal and return from it, no actions done
 */
static void signaltest_basic_signal(void) {
    int pid = syscreate(&testfunc, DEFAULT_STACK_SIZE);
    ASSERT(pid > 0);
    kprintf("created %d\n", pid);

    // let testfunc setup its syshandler
    sysyield();
    int result = syskill(pid, 0);
    kprintf("syskill returned: %d\n", result);
    sysyield();
}

void testfunc(void) {
    funcptr_args1 oldHandler;
    funcptr_args1 newhandler = &print_signal;
    int ret = syssighandler(0, newhandler, &oldHandler);
    kprintf("sighandler returned: %d\n", ret);
    sysyield();
    kprintf("return to testfunc\n");
}

void print_signal(void* cntx) {
    (void)cntx;
    kprintf("print signal called!\n");
}
