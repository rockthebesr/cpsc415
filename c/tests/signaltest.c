/* signaltest.c : test code for signal handling

Called from outside:
  signal_run_all_tests() - runs all signal handling tests
*/

#include <xerostest.h>
#include <xeroskernel.h>

// Tests
static void signaltest_syskill(void);
static void signaltest_syshandler(void);
static void signaltest_signal_priorities(void);
static void signaltest_signal_blocked(void);

// Helpers
static void setup_signal_handler(funcptr_args1 newhandler);
static void basic_signal_handler(void* cntx);
static void basic_test_func(void);
static void test_blocked(void);
static void test_priorities(void);
static void dummy_proc(void);
static void low_pri(void* cntx);
static void medium_pri(void* cntx);
static void high_pri(void* cntx);
static void nop_handler(void* cntx);
static void useless_func(void);

static int g_signal_fired = 0;

/**
 * Runs all signal tests.
 */
void signal_run_all_tests(void) {
    signaltest_syskill();
    signaltest_syshandler();
    signaltest_signal_priorities();
    signaltest_signal_blocked();
    DEBUG("Done all signal tests. Looping forever\n");
    while(1);
}

/**
 * Tests that we can send a signal and return from it, no actions done
 */
static void signaltest_syskill(void) {
    int pid = syscreate(&basic_test_func, DEFAULT_STACK_SIZE);
    ASSERT(pid > 0);

    // setup signal handler in basic_test_func
    sysyield();

    // syskill error cases
    ASSERT_EQUAL(syskill(-1, 0), SYSKILL_TARGET_DNE);
    ASSERT_EQUAL(syskill(9999, 0), SYSKILL_TARGET_DNE);
    ASSERT_EQUAL(syskill(pid, -1), SYSKILL_INVALID_SIGNAL);
    ASSERT_EQUAL(syskill(pid, 32), SYSKILL_INVALID_SIGNAL);

    // test signal works
    int result = syskill(pid, 11);
    ASSERT_EQUAL(result, 0);

    result = syskill(pid, 0);
    ASSERT_EQUAL(result, 0);
    sysyield();
}

/**
 * Tests error responses of syshandler, and all other basics
 */
static void signaltest_syshandler(void) {
    funcptr_args1 oldHandler;

    // error codes
    ASSERT_EQUAL(syssighandler(-1, &low_pri, &oldHandler),
                 SYSHANDLER_INVALID_SIGNAL);
    ASSERT_EQUAL(syssighandler(32, &low_pri, &oldHandler),
                 SYSHANDLER_INVALID_SIGNAL);
    ASSERT_EQUAL(syssighandler(0, &low_pri, NULL),
                 SYSHANDLER_INVALID_FUNCPTR);
    ASSERT_EQUAL(syssighandler(0, (funcptr_args1)NULL, &oldHandler),
                 SYSHANDLER_INVALID_FUNCPTR);

    // change signal handlers, get back old ones
    setup_signal_handler(&low_pri);
    ASSERT_EQUAL(syssighandler(0, &high_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, low_pri);
    ASSERT_EQUAL(syssighandler(31, &high_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, NULL);
    ASSERT_EQUAL(syssighandler(31, &low_pri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, high_pri);
}

/**
 * Ensures signals fire in priority order
 */
static void signaltest_signal_priorities(void) {
    int pid = syscreate(&test_priorities, DEFAULT_STACK_SIZE);
    ASSERT(pid > 0);

    // setup signal handlers
    sysyield();

    int result = syskill(pid, 0);
    ASSERT_EQUAL(result, 0);
    result = syskill(pid, 15);
    ASSERT_EQUAL(result, 0);
    result = syskill(pid, 31);
    ASSERT_EQUAL(result, 0);
    sysyield();
}

/**
 * Signalling blocked procs should fail the syscall, return special code
 */
static void signaltest_signal_blocked(void) {
    int pid = syscreate(&test_blocked, DEFAULT_STACK_SIZE);
    sysyield();

    syskill(pid, 0);
    sysyield();
    syskill(pid, 0);
    sysyield();
    syskill(pid, 0);
}

/**
 * Simple helper to set up newhandler as the lowest priority signal handler
 */
static void setup_signal_handler(funcptr_args1 newhandler) {
    funcptr_args1 oldHandler;
    int ret = syssighandler(0, newhandler, &oldHandler);
    ASSERT_EQUAL(ret, 0);
}

/* test processes to run the signal handlers */

static void basic_test_func(void) {
    setup_signal_handler(&basic_signal_handler);

    sysyield();
    ASSERT_EQUAL(g_signal_fired, 1);

    // check we can run syscalls after signal handling
    ASSERT(sysgetpid() > 1);
    sysstop();
    ASSERT(0);
}


static void test_priorities(void) {
    g_signal_fired = 0;
    setup_signal_handler(&low_pri);

    funcptr_args1 oldHandler;
    int ret = syssighandler(31, &high_pri, &oldHandler);
    ASSERT_EQUAL(ret, 0);

    ret = syssighandler(15, &medium_pri, &oldHandler);
    ASSERT_EQUAL(ret, 0);

    // allow parent to call syskill
    sysyield();

    // trigger our signal handlers
    // expect to see g_signal_fired go 0 -> deadbeef -> beefbeef -> cafecafe
    sysyield();
    ASSERT_EQUAL(g_signal_fired, 0xCAFECAFE);
}

static void test_blocked(void) {
    setup_signal_handler(&nop_handler);
    int pid = syscreate(&dummy_proc, DEFAULT_STACK_SIZE);
    unsigned long num = 0xA5A5A5A5;

    // test send, recv, recv_any
    ASSERT_EQUAL(syssend(pid, num), PROC_SIGNALLED);
    ASSERT_EQUAL(sysrecv(&pid, &num), PROC_SIGNALLED);
    pid = 0;
    ASSERT_EQUAL(sysrecv(&pid, &num), PROC_SIGNALLED);
}

static void dummy_proc(void) {
    while(1) {
        sysyield();
    }
}

/* signal handlers */

static void basic_signal_handler(void* cntx) {
    (void)cntx;
    g_signal_fired = 1;
}

static void low_pri(void* cntx) {
    ASSERT_EQUAL(g_signal_fired, 0xBEEFBEEF);
    g_signal_fired = 0xCAFECAFE;
}

static void medium_pri(void* cntx) {
    ASSERT_EQUAL(g_signal_fired, 0xDEADBEEF);
    g_signal_fired = 0xBEEFBEEF;
}

static void high_pri(void* cntx) {
    ASSERT_EQUAL(g_signal_fired, 0);
    g_signal_fired = 0xDEADBEEF;
}

static void nop_handler(void* cntx) {
    (void)cntx;
    // Do some garbage calls, to test we maintain the ret value on the stack
    useless_func();
    processStatuses ps;
    int num_procs_before = sysgetcputimes(&ps);
    ASSERT_EQUAL(syssend(12345, num_procs_before), SYSPID_DNE);
    ASSERT(sysgetpid() >= 1);
    sysyield();
}

static void useless_func(void) {
    for (int i = 0; i < 50; i++);
}
