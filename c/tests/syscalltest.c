/* syscalltest.c : test code for syscalls

Called from outside:
  syscall_run_all_tests() - runs all syscall tests

*/

#include <xerostest.h>
#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

/**
 * syscalltests
 */
static void syscalltest1_create_max_number_of_processes(void);
static void syscalltest2_create_bad_params(void);
static void syscalltest3_fibonacci_test(void);
static void test_sysgetpid(void);
static void test_sysputs(void);
static void test_syswait(void);

/**
 * Helper functions for test cases
 */
static void testfunc(void);
static int fibonacci(int num);
static void syscall_fibonacci_test_func1(void);
static void syscall_fibonacci_test_func2(void);
static void syscall_fibonacci_test_func3(void);
static void sysgetpid_proc(void);

/**
 * Runs all syscall tests
 */
void syscall_run_all_tests(void) {
    // Should be first test run - simplifies internal testing logic
    test_sysgetpid();

    // Call this test multiple times to verify cleanup and PCB reuse
    syscalltest1_create_max_number_of_processes();
    syscalltest1_create_max_number_of_processes();
    syscalltest1_create_max_number_of_processes();
    
    syscalltest2_create_bad_params();
    
    syscalltest3_fibonacci_test();

    test_sysputs();

    test_syswait();
    
    kprintf("Done syscall_run_all_tests, looping forever.\n");
    while(1);
}

/**
 * Test process function for syscalltest1_create_max_number_of_processes
 */
static void testfunc(void) {
    kprintf("testfunc starting\n");
    kprintf("testfunc ending\n");
    // Calling return on a syscreated fuction will automatically jump to sysstop()
    return;
    
    // Should never get here
    kprintf("Code continued executing after sysstop()\n");
    ASSERT(0);
}

/**
 * Test: Process Bomb
 * Keep creating new processes until we hit the limit.
 * Verify the correct error code is returned
 * Clean up the processes afterward.
 */
static void syscalltest1_create_max_number_of_processes(void) {
    int result;
    int count = 0;
    
    do {
        result = syscreate(&testfunc, DEFAULT_STACK_SIZE);
        count++;
    } while (result >= 1);
    kprintf("Result: %d\nSpawned %d processes\n", result, count);
    ASSERT_EQUAL(result, EPROCLIMIT);
    ASSERT_EQUAL(count, PCB_TABLE_SIZE);

    MASS_SYSYIELD();
    kprintf("%s complete\n", __func__);
}

/**
 * Test: syscreate bad parameters
 * Pass invalid parameters and verify the correct return code is produced
 */
static void syscalltest2_create_bad_params(void) {
    int result;
    
    result = syscreate((funcptr)-1, DEFAULT_STACK_SIZE);
    ASSERT_EQUAL(result, EINVAL);
    
    result = syscreate((funcptr)0, DEFAULT_STACK_SIZE);
    ASSERT_EQUAL(result, EINVAL);
    
    result = syscreate((funcptr)NULL, DEFAULT_STACK_SIZE);
    ASSERT_EQUAL(result, EINVAL);
    
    result = syscreate((funcptr)(kmem_maxaddr()), DEFAULT_STACK_SIZE);
    ASSERT_EQUAL(result, EINVAL);
    
    result = syscreate((funcptr)(kmem_maxaddr() + 1), DEFAULT_STACK_SIZE);
    ASSERT_EQUAL(result, EINVAL);
    
    result = syscreate(&testfunc, kmem_maxaddr());
    ASSERT_EQUAL(result, ENOMEM);
    kprintf("%s complete\n", __func__);
}

/**
 * Test function which triggers a ton of sysyields()
 */
static int fibonacci(int num) {
    sysyield();
    
    if (num <= 2) {
        return 1;
    }
    
    return fibonacci(num - 1) + fibonacci(num - 2);
}

/**
 * Test function for syscalltest3_fibonacci_test()
 */
static void syscall_fibonacci_test_func1(void) {
    // Test to make sure syscreate + sysyield won't mess up our stack
    kprintf("%s: %d = %d\n", __func__, 1, fibonacci(1));
    kprintf("%s: %d = %d\n", __func__, 2, fibonacci(2));
    kprintf("%s: %d = %d\n", __func__, 4, fibonacci(4));
    kprintf("%s: %d = %d\n", __func__, 8, fibonacci(8));
    sysstop();
    
    // Should never get here
    kprintf("Code continued executing after sysstop()\n");
    ASSERT(0);
}

/**
 * Test function for syscalltest3_fibonacci_test()
 */
 static void syscall_fibonacci_test_func2(void) {
    // Test to make sure syscreate + sysyield won't mess up our stack
    kprintf("%s: %d = %d\n", __func__, 1, fibonacci(1));
    kprintf("%s: %d = %d\n", __func__, 9, fibonacci(9));
    kprintf("%s: %d = %d\n", __func__, 6, fibonacci(6));
    kprintf("%s: %d = %d\n", __func__, 3, fibonacci(3));
    sysstop();
    
    // Should never get here
    kprintf("Code continued executing after sysstop()\n");
    ASSERT(0);
}

/**
 * Test function for syscalltest3_fibonacci_test()
 */
 static void syscall_fibonacci_test_func3(void) {
    // Test to make sure syscreate + sysyield won't mess up our stack
    kprintf("%s: %d = %d\n", __func__, 1, fibonacci(1));
    kprintf("%s: %d = %d\n", __func__, 1, fibonacci(1));
    kprintf("%s: %d = %d\n", __func__, 1, fibonacci(1));
    kprintf("%s: %d = %d\n", __func__, 1, fibonacci(1));
    sysstop();
    
    // Should never get here
    kprintf("Code continued executing after sysstop()\n");
    ASSERT(0);
}

/**
 * Test: Stack usage and multiprocessing
 * Have numerous programs make many nested function calls while
 * interleaving their execution.
 */
 static void syscalltest3_fibonacci_test(void) {
    syscreate(&syscall_fibonacci_test_func1, DEFAULT_STACK_SIZE);
    syscreate(&syscall_fibonacci_test_func2, DEFAULT_STACK_SIZE);
    syscreate(&syscall_fibonacci_test_func3, DEFAULT_STACK_SIZE);

    MASS_SYSYIELD();
    kprintf("%s complete\n", __func__);
}

// Unfortuantly somewhat hacky way of testing pids
static int next_test_pid;

/**
 * Creates a few processes which assert their pid is the expected one
 */
static void test_sysgetpid(void) {
    kprintf("testing sysgetpid()...\n");

    next_test_pid = sysgetpid();
    kprintf("syscalltest's pid: %d\n", next_test_pid);
    ASSERT(next_test_pid > 0);
    next_test_pid++;

    for (int i = 0; i < 10; i++) {
        syscreate(&sysgetpid_proc, DEFAULT_STACK_SIZE);
        sysyield();
    }
}

/**
 * created by test_sysgetpid()
 */
static void sysgetpid_proc(void) {
    ASSERT_EQUAL(next_test_pid, sysgetpid());
    next_test_pid++;
    return;
}

/**
 * Tests sysputs(). Output to be verified by user
 */
static void test_sysputs(void) {
    kprintf("testing sysputs...\n");
    BUSYWAIT();

    kprintf("Nothing should be printed here:\n");
    sysputs((char*)-1);
    sysputs((char*)(kmem_maxaddr() + 1));

    kprintf("Everything here should print:\n");
    sysputs("This\n");
    sysputs("is a\n");
    sysputs("test\n");

    char *str1 = "Hello world!\n";
    sysputs(str1);

    char str2[80];
    int class = 415;
    sprintf(str2, "Class is CPSC %d\n", class);
    sysputs(str2);

    BUSYWAIT();
}

/**
 * Tests syswait() basic functionality and error responses
 */
static void test_syswait(void) {
    kprintf("syswait checking invalid pids\n");
    ASSERT_EQUAL(syswait(-1), SYSPID_DNE);
    ASSERT_EQUAL(syswait(9000), SYSPID_DNE);

    kprintf("creating process, waiting on it\n");
    int pid = syscreate(&testfunc, DEFAULT_STACK_SIZE);
    ASSERT_EQUAL(syswait(pid), 0);
    kprintf("returned from wait\n");
}
