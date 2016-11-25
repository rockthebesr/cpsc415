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
static void test_sysgetcputimes(void);

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
static void cputimehelper(void);
static int syskill_wrapper(int pid);

/**
 * Runs all timer tests
 */
void timer_run_all_tests(void) {
    initPIT(1000 / TICK_LENGTH_IN_MS);
    test_preemption();
    test_preemption2();
    test_rand_timesharing();
    //test_sysgetcputimes();
    test_sleep1_simple();
    //test_sleep2_killmid();
    test_sleep3_simultaneous_wake();
    test_idleproc();
}

// Used in a few tests as a makeshift semaphore
static int count = 0;

/**
 * Sets up termination signal handler, calls signal on proc
 */
static int syskill_wrapper(int pid) {
    (void)pid;
    // TODO setup handler to systop
    return syskill(pid, 31);
}

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

    // join
    pid = 0;
    unsigned long num;
    sysrecv(&pid, &num);
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
    syskill_wrapper(pids[0]);
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

/**
 * Tests sysgetcputimes()
 */
static void test_sysgetcputimes(void) {
    // give all other procs a chance to die before we run
    sysyield();

    // invalid addresses
    ASSERT_EQUAL(sysgetcputimes((processStatuses*)-8), -1);
    ASSERT_EQUAL(sysgetcputimes((processStatuses*)0xFFFFFFFF), -1);

    // falls within hole
    ASSERT_EQUAL(sysgetcputimes((processStatuses*)HOLESTART), -1);

    processStatuses ps;

    int num_procs_before = sysgetcputimes(&ps);
    ASSERT(num_procs_before > 0 && num_procs_before < 32);
    kprintf("num_procs_before in sysgetcputimes: %d\n", num_procs_before);

    int proc_pid_1 = syscreate(&cputimehelper, DEFAULT_STACK_SIZE);
    ASSERT(proc_pid_1 > 0);

    int proc_pid_2 = syscreate(&cputimehelper, DEFAULT_STACK_SIZE);
    ASSERT(proc_pid_2 > 0);

    // created then killed to ensure stopped procs aren't in ps
    int proc_pid_3 = syscreate(&cputimehelper, DEFAULT_STACK_SIZE);
    ASSERT(proc_pid_3 > 0);
    ASSERT_EQUAL(syskill_wrapper(proc_pid_3), 0);

    // call sysyield so created procs can run
    sysyield();

    int num_procs_after = sysgetcputimes(&ps);

    // check procs are added, stopped processes aren't
    ASSERT_EQUAL(num_procs_before + 2, num_procs_after);

    // idle proc check
    ASSERT_EQUAL(ps.pid[0], 0);
    ASSERT_EQUAL(ps.status[0], PROC_STATE_READY);
    ASSERT(ps.cpuTime[0] >= 0);

    // current proc check - should be proc 1, timertest should be dispatched
    ASSERT(ps.pid[1] >= 1);
    ASSERT_EQUAL(ps.status[1], PROC_STATE_RUNNING);
    ASSERT(ps.cpuTime[1] >= 1);

    int hit_1 = 0;
    int hit_2 = 0;
    for (int i = 2; i <= num_procs_after; i++) {
        if (!hit_1 && ps.pid[i] == proc_pid_1) {
            hit_1 = 1;
            ASSERT(ps.cpuTime[i] >= 1);
        } else if (!hit_2 && ps.pid[i] == proc_pid_2) {
            hit_2 = 1;
            ASSERT(ps.cpuTime[i] >= 1);
        } else {
            ASSERT(ps.cpuTime[i] >= 0);
        }

        ASSERT_EQUAL(ps.status[i], PROC_STATE_READY);
    }

    ASSERT(hit_1 && hit_2);

    kprintf("done test_sysgetcputimes()\n");
}

/**
 * Helper for test_sysgetcputime
 */
static void cputimehelper(void) {
    BUSYWAIT();
    BUSYWAIT();
}
