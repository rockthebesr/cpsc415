/* memtest.c : test code for memory manager

Called from outside:
  mem_run_all_tests() - runs all memory management tests

*/

#include <xerostest.h>
#include <xeroskernel.h>
#include <i386.h>
#include <stdlib.h>
#include <limits.h>

extern long freemem;

static void mem_simple_test_1(void);
static void mem_simple_test_2(void);
static void mem_stress_test_1(void);
static void mem_stress_test_2(void);
static void mem_test_split_coalesce_blocks_1(void);
static void initial_free_list_check(void);

/**
 * Runs all memory management tests.
 */
void mem_run_all_tests(void) {
    mem_simple_test_1();
    mem_simple_test_2();
    mem_stress_test_1();
    mem_stress_test_2();
    mem_test_split_coalesce_blocks_1();
    DEBUG("Done all mem tests. Looping forever\n");
    while(1);
}

/**
 * As name implies, simple test to verify free list is working,
 * verified through kmem_dump_free_list() output.
 * Of note, this test also checks that we allocate on 16 byte boundaries.
 */
static void mem_simple_test_1(void) {
    kprintf("Running mem_simple_test_1\n");

    kmem_dump_free_list();

    void *p1 = kmalloc(1);
    void *p2 = kmalloc(2);

    kmem_dump_free_list();
    BUSYWAIT();

    kfree(p1);
    kfree(p2);
    void *p3 = kmalloc(3);
    kfree(p3);

    kmem_dump_free_list();
    BUSYWAIT();
}

/**
 * Tests some basic error handling by kmalloc, kfree
 */
static void mem_simple_test_2(void) {
    kfree(NULL);
    ASSERT_EQUAL(kmalloc(-1), NULL);
    ASSERT_EQUAL(kmalloc(0), NULL);
 
    // check for overflows
    ASSERT_EQUAL(kmalloc(INT_MAX), NULL);
}

/**
 * At the start, free memory broken into 2 blocks: before hole, and after.
 * This test stresses the memory management by continuously allocating and
 * freeing these blocks. No longer test before hole; kmalloc is used in initproc
 */
static void mem_stress_test_1(void) {
    kprintf("Running mem_stress_test_1\n");

    void *block_2_addr = (void*)0x196010;
    long block_2_size = 0x269fe0;
    void **p2;

    for (int i = 0; i < 100; i++) {
        p2 = kmalloc(block_2_size);
        ASSERT_EQUAL(p2, block_2_addr);
        kfree(p2);
    }

    initial_free_list_check();
    kprintf("mem_stress_test_1 passed\n");
}

/**
 * These functions are hidden, but we can test them through examining
 * the output of kmem_dump_free_list().
 */
static void mem_test_split_coalesce_blocks_1(void) {
    kprintf("running mem_test_split_coalesce_blocks_1\n");
    initial_free_list_check();

    void *p1 = kmalloc(0x1000);
    void *p2 = kmalloc(0x2000);

    // Initial size of first block should be decreased by 0x3000 + 0x10*2
    // Should see 2 blocks
    kmem_dump_free_list();
    BUSYWAIT();

    kfree(p1);

    // Should see 3 blocks - new first one, p1
    kmem_dump_free_list();

    kfree(p2);

    // Memory should be fully coalesced
    kprintf("This free list should match initial free list:\n");
    initial_free_list_check();
}

/**
 * Similar to previous test, except takes a more stress-test approach,
 * and assertions, rather than the user, does the testing.
 */
static void mem_stress_test_2(void) {
    kprintf("Running mem_stress_test_2\n");

    initial_free_list_check();

    void *ptr_arr[2000];

    for (int i = 0; i < 2000; i++) {
        // diverse block sizes through modular arithmetic
        ptr_arr[i] = kmalloc(((0x1000 * i) % 317) + 1);
        ASSERT(ptr_arr[i] != NULL);
    }

    // Free first half in forward order
    for (int i = 0; i < 2000; i+=2) {
        kfree(ptr_arr[i]);
    }

    kmem_dump_free_list();
    ASSERT_EQUAL(kmem_get_free_list_length(), 1002);

    // Free remaining blocks in reverse order
    for (int i = 1999; i >= 1; i-=2) {
        kfree(ptr_arr[i]);
    }

    // Memory should be fully coalesced
    kprintf("This free list should match initial free list:\n");
    initial_free_list_check();
    kprintf("mem_stress_test_2 passed\n");
}

/**
 * Helper method to check common start/end conditions of tests
 */
static void initial_free_list_check(void) {
    ASSERT_EQUAL(kmem_get_free_list_length(), 2);
    kmem_dump_free_list();
    BUSYWAIT();
}
