/* copyinouttest.c: tests for copyinout, which ensures user pointers are safe

Called from outside:
  copyinout_run_all_tests() - runs all tests in this file   
*/

#include <xeroskernel.h>
#include <xeroslib.h>
#include <xerostest.h>
#include <i386.h>
#include <copyinout.h>

static void test_verify_usrptr(void);
static void test_verify_usrstr(void);

/**
 * Runs all copyinout tests
 */
void copyinout_run_all_tests(void){
    kprintf("Testing copyinout\n");
    test_verify_usrptr();
    test_verify_usrstr();
    kprintf("Done all copyinout tests. Looping forever\n");
    while(1);
}

/**
 * Ensures a user's pointer to a variable is safe
 */
static void test_verify_usrptr(void) {
    kprintf("Testing verify_usrptr()\n");
    void *kstack_start = (void*)(kmem_freemem() - KERNEL_STACK);
    void *kstack_end = (void*)(kmem_freemem());

    // bounds checking
    ASSERT_EQUAL(verify_usrptr((void*)-1, 1), EINVAL);
    ASSERT_EQUAL(verify_usrptr(NULL, 1), EINVAL);
    ASSERT_EQUAL(verify_usrptr((void*)kmem_maxaddr() - 4, 8), EINVAL);

    // falls in hole
    ASSERT_EQUAL(verify_usrptr((void*)(HOLESTART + 4), 4), EINVAL);
    ASSERT_EQUAL(verify_usrptr((void*)(HOLEEND - 4), 4), EINVAL);
    ASSERT_EQUAL(verify_usrptr((void*)(HOLEEND - 4), 8), EINVAL);
    ASSERT_EQUAL(verify_usrptr((void*)(HOLESTART), 4), EINVAL);
    ASSERT_EQUAL(verify_usrptr((void*)(HOLESTART - 4), 8), EINVAL);


    // falls in kstack
    ASSERT_EQUAL(verify_usrptr(kstack_start + 4, 4), EINVAL);
    ASSERT_EQUAL(verify_usrptr(kstack_end - 4, 4), EINVAL);
    ASSERT_EQUAL(verify_usrptr(kstack_end - 4, 8), EINVAL);
    ASSERT_EQUAL(verify_usrptr(kstack_start, 4), EINVAL);
    ASSERT_EQUAL(verify_usrptr(kstack_start - 4, 8), EINVAL);

    // addresses that work
    ASSERT_EQUAL(verify_usrptr((void*)kmem_maxaddr(), 1), OK);
    ASSERT_EQUAL(verify_usrptr(kstack_start - 8, 4), OK);
    ASSERT_EQUAL(verify_usrptr(kstack_end, 4), OK);
    ASSERT_EQUAL(verify_usrptr((void*)kmem_maxaddr() - 8, 8), OK);
    ASSERT_EQUAL(verify_usrptr((void*)kmem_maxaddr() - 8, 4), OK);
}

/**
 * Ensures a user's pointer to a null-terminated string is safe
 */
static void test_verify_usrstr(void) {
    kprintf("Testing verify_usrstr()\n");
    char *old_data = kmalloc(32);
    char *new_data = "test\0";
    char user_stack = '\0';

    // Bounds checking:
    // Since verify_usrstr relies on verify_usrptr,
    // we assume hole/stack/memory range work if one works(same implementation).
    ASSERT_EQUAL(verify_usrstr((void*)-1), EINVAL);
    ASSERT_EQUAL(verify_usrstr((void*)kmem_maxaddr() + 1), EINVAL);

    strncpy(old_data, (char*)kmem_maxaddr() - 4, 5);
    strncpy((char*)kmem_maxaddr() - 4, new_data, 5);
    ASSERT_EQUAL(verify_usrstr((void*)kmem_maxaddr()), OK);
    ASSERT_EQUAL(verify_usrstr((void*)kmem_maxaddr() - 4), OK);
    strncpy((char*)kmem_maxaddr(), old_data, 4);

    // typical addresses we expect
    strcpy(old_data, "hello world\0");
    ASSERT_EQUAL(verify_usrstr(old_data), OK);
    *old_data = '\0';
    ASSERT_EQUAL(verify_usrstr(old_data), OK);
    ASSERT_EQUAL(verify_usrstr(&user_stack), OK);

    kfree(old_data);
}
