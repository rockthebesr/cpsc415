/* signaltest.c : test code for signal handling

Called from outside:
  signal_run_all_tests() - runs all signal handling tests
*/

#include <xerostest.h>
#include <xeroskernel.h>

// Tests
static void signaltest_basic_signal(void);
static void signaltest_syshandler(void);
static void signaltest_signal_priorities(void);

// Helpers
static void setup_signal_handler(funcptr_args1 newhandler);
static void basic_signal_handler(void* cntx);
static void basic_test_func(void);
static void test_priorities(void);
static void lowPri(void);
static void mediumPri(void);
static void highPri(void);

static int g_signal_fired = 0;

/**
 * Runs all signal tests.
 */
void signal_run_all_tests(void) {
    signaltest_basic_signal();
    signaltest_syshandler();
    signaltest_signal_priorities();
    DEBUG("Done all signal tests. Looping forever\n");
    while(1);
}

/**
 * Tests that we can send a signal and return from it, no actions done
 */
static void signaltest_basic_signal(void) {
    int pid = syscreate(&basic_test_func, DEFAULT_STACK_SIZE);
    ASSERT(pid > 0);

    // setup signal handler in basic_test_func
    sysyield();

    // test signal works
    int result = syskill(pid, 0);
    ASSERT_EQUAL(result, 0);
    sysyield();
}

/**
 * Tests error responses of syshandler, and all other basics
 */
static void signaltest_syshandler(void) {
    funcptr_args1 oldHandler;

    // error codes
    ASSERT_EQUAL(syssighandler(-1, &lowPri, &oldHandler),
                 SYSHANDLER_INVALID_SIGNAL);
    ASSERT_EQUAL(syssighandler(32, &lowPri, &oldHandler),
                 SYSHANDLER_INVALID_SIGNAL);
    ASSERT_EQUAL(syssighandler(0, &lowPri, (funcptr_args1)NULL),
                 SYSHANDLER_INVALID_FUNCPTR);
    ASSERT_EQUAL(syssighandler(0, (funcptr_args1)NULL, &oldHandler),
                 SYSHANDLER_INVALID_FUNCPTR);

    // change signal handlers, get back old ones
    setup_signal_handler(&lowPri);
    ASSERT_EQUAL(syssighandler(0, &highPri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, lowPri);
    ASSERT_EQUAL(syssighandler(31, &highPri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, NULL);
    ASSERT_EQUAL(syssighandler(31, &lowPri, &oldHandler), 0);
    ASSERT_EQUAL(oldHandler, highPri);
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
}


static void test_priorities(void) {
    g_signal_fired = 0;
    setup_signal_handler(&lowPri);

    funcptr_args1 oldHandler;
    int ret = syssighandler(31, &highPri, &oldHandler);
    ASSERT_EQUAL(ret, 0);

    ret = syssighandler(15, &mediumPri, &oldHandler);
    ASSERT_EQUAL(ret, 0);

    // allow parent to call syskill
    sysyield();

    // trigger our signal handlers
    // expect to see g_signal_fired go 0 -> deadbeef -> beefbeef -> cafecafe
    sysyield();
    ASSERT_EQUAL(g_signal_fired, 0xCAFECAFE);
}

/* signal handlers */

static void basic_signal_handler(void* cntx) {
    (void)cntx;
    g_signal_fired = 1;
}

static void lowPri(void) {
    ASSERT_EQUAL(g_signal_fired, 0xBEEFBEEF);
    g_signal_fired = 0xCAFECAFE;
}

static void mediumPri(void) {
    ASSERT_EQUAL(g_signal_fired, 0xDEADBEEF);
    g_signal_fired = 0xBEEFBEEF;
}

static void highPri(void) {
    ASSERT_EQUAL(g_signal_fired, 0);
    g_signal_fired = 0xDEADBEEF;
}
