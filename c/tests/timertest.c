/* timertest.c : test code for kernel pre-emption

Called from outside:
  timer_run_all_tests() - runs all timer tests

*/

#include <xerostest.h>
#include <xeroskernel.h>
#include <xeroslib.h>
#include <i386.h>

/**
 * timertests
 */
static void test_preemption(void);
static void test_preemption2(void);
static void test_idleproc(void);
static void test_sleep1_simple(void);
static void test_sleep2_killmid(void);
static void test_sleep3_simultaneous_wake(void);
static void test_rand_timesharing(void);

/**
 * Helper functions for test cases
 */
static void addOdds(void);
static void addEvens(void);
static void counter(void);
static void testdeadlock(void);
static void sleep5(void);
static void sleep10(void);
static void sleep20(void);
static void rand_sleep_and_print(void);

/**
 * Runs all timer tests
 */
void timer_run_all_tests(void) {
    initPIT(TICK_LENGTH_IN_MS * 10);
    
    test_preemption();
    test_preemption2();
    test_rand_timesharing();
    test_sleep1_simple();
    test_sleep2_killmid();
    test_sleep3_simultaneous_wake();
    test_idleproc();
}

// Used in a few tests as a makeshift semaphore
static int count = 0;

/**
 * Tests that we can get work done, despite being uncooperative
 */
static void test_preemption(void) {
    int result;
    result = syscreate(&addOdds, DEFAULT_STACK_SIZE);
    ASSERT(result >= 1);

    result = syscreate(&addEvens, DEFAULT_STACK_SIZE);   
    ASSERT(result >= 1);

    while(count < 10);

    MASS_SYSYIELD();

    kprintf("test_preemption passed!\n");
}

/**
 * Created by test_preemption()
 */
static void addOdds(void) {
    while(count < 10) {
        if (count % 2 == 0) {
            count++;
            kprintf("count: %d\n", count);
        }
    }
}

/**
 * Created by test_preemption()
 */
static void addEvens(void) {
    while(count < 10) {
        if (count % 2 != 0) {
            count++;
            kprintf("count: %d\n", count);
        }
    }
}

/**
 * Created by test_preemption2()
 */
static void counter(void) {
    int pid = sysgetpid();
    char buf[80];
    for (int i = 0; i < 20; i++) {
        sprintf(buf, "{pid%d: %d} ", pid, i);
        sysputs(buf);
    }
    sprintf(buf, "{pid%d: done} ", pid);
    sysputs(buf);
}

/**
 * Tests time sharing.
 * 5 processes will uncooperatively try to count to 20.
 * The output of this test should demonstrate their execution interleaving
 */
static void test_preemption2(void) {
    int pids[10];
    for (int i = 0; i < 5; i++) {
        pids[i] = syscreate(&counter, DEFAULT_STACK_SIZE);
        ASSERT(pids[i] > 0);
    }
    
    // Join all processes
    for (int i = 0; i < 10; i++) {
        syssend(pids[i], 0xCAFECAFE);
    }
    sysputs("\n");
    DEBUG("Done.\n");
}

// Required by test_idleproc()
static int test_idleproc_pid;

/**
 * Creates a deadlock, prompting the idleproc to start
 */
static void test_idleproc(void) {
    kprintf("Testing idle proc. If this passes, we'll loop forever.\n");

    test_idleproc_pid = sysgetpid();

    int pid = syscreate(&testdeadlock, DEFAULT_STACK_SIZE);
    ASSERT(pid >= 1);

    int num = 0xdeadbeef;
    syssend(pid, num);

    ASSERT(0);
}

/**
 * Created by test_idleproc
 */
static void testdeadlock(void) {
    int num = 0xdeadbeef;
    syssend(test_idleproc_pid, num);
}

/**
 * Simple test of syssleep() to ensure basics works
 */
static void test_sleep1_simple(void) {
    int result;
    count = 0;

    result = syscreate(&sleep10, DEFAULT_STACK_SIZE);
    ASSERT(result >= 1);

    result = syscreate(&sleep20, DEFAULT_STACK_SIZE);
    ASSERT(result >= 1);
    
    result = syscreate(&sleep5, DEFAULT_STACK_SIZE);
    ASSERT(result >= 1);
    

    while(count < 3) {
        sysputs("\tStill waiting...\n");
        syssleep(1000);
    }
    
    sysputs("Done test_sleep1_simple\n");
}

/**
 * Test the delta queue in case we kill an entry that is sleeping
 */
static void test_sleep2_killmid(void) {
    int pids[3];
    count = 0;

    pids[0] = syscreate(&sleep10, DEFAULT_STACK_SIZE);
    ASSERT(pids[0] >= 1);

    pids[1] = syscreate(&sleep20, DEFAULT_STACK_SIZE);
    ASSERT(pids[1] >= 1);
    
    pids[2] = syscreate(&sleep5, DEFAULT_STACK_SIZE);
    ASSERT(pids[2] >= 1);
    
    syssleep(1000);
    sysputs("Killing sleep10...");
    syskill(pids[0]);
    sysputs("Done.\n");

    while(count < 2) {
        sysputs("\tStill waiting...\n");
        syssleep(1000);
    }
    
    sysputs("Done test_sleep2_killmid\n");
}

static void test_sleep3_simultaneous_wake(void) {
    int i;
    count = 0;
    
    for (i = 0; i < 3; i++) {
        ASSERT(syscreate(&sleep5, DEFAULT_STACK_SIZE) > 0);
    }
    
    while(count < 3) {
        sysputs("\tStill waiting...\n");
        syssleep(1000);
    }
    
    sysputs("Done test_sleep3_simultaneous_wake\n");
}

/* These are all for the sleep tests, to call syssleep() for a preset time */
static void sleep5(void) {
    syssleep(5000);
    sysputs("Slept: 5000\n");
    count++;
}

static void sleep10(void) {
    syssleep(10000);
    sysputs("Slept: 10000\n");
    count++;
}

static void sleep20(void) {
    syssleep(20000);
    sysputs("Slept: 20000\n");
    count++;
}

/**
 * Helper test for test_rand_timesharing()
 */
static void rand_sleep_and_print(void) {
    int pid = sysgetpid();
    char buf[80];
    unsigned long sleep;
    int i;
    
    for (i = 0; i < 5; i++) {
        sleep = (rand() % 5) * 100;
        sprintf(buf, "pid %d: (%d) sleeping for %lums\n", pid, i, sleep);
        sysputs(buf);
        syssleep(sleep);
    }
    sprintf(buf, "pid %d: DONE\n", pid, i);
    sysputs(buf);
}

/**
 * Create 5 processes which print and then sleep for a random amount of time
 */
static void test_rand_timesharing(void) {
    int childpid[5];
    int i;
    
    for (i = 0; i < 5; i++) {
        childpid[i] = syscreate(&rand_sleep_and_print, DEFAULT_STACK_SIZE);
    }
    
    // Really trashy "join". This loop terminates when all children are done
    for (i = 0; i < 5; i++) {
        syssend(childpid[i], 0xcafecafe);
    }
    
    DEBUG("Done!\n");
}