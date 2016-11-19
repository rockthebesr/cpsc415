/* disptest.c : test code for dispatcher's pcb handling

Called from outside:
  disp_run_all_tests() - runs all tests in this file

*/

#include <xerostest.h>
#include <xeroskernel.h>
#include <pcb.h>

static void test_full_table(void);
static void test_change_queue(void);
static void test_get_next_proc(void);

static int create_test_proc(void);
static void cleanup_queue(proc_state_enum_t queue);
static void reset_pcb_table(void);
static void dummy(void);

extern proc_ctrl_block_t g_pcb_table[PCB_TABLE_SIZE];
extern proc_ctrl_block_t *g_proc_queue_heads[2];

/**
 * Runs all dispatcher tests
 */
void disp_run_all_tests(void){
    test_get_next_proc();
    test_full_table();
    test_change_queue();
    DEBUG("Done all queue tests. Looping forever\n");
    while(1);
}

/**
 * Ensures that we can handle a full pcb table, tests get_next_available_pcb()
 */
static void test_full_table(void) {
    // Fill up PCB table
    for (int i = 0; i < PCB_TABLE_SIZE; i++) {
        ASSERT(create_test_proc() >= 1);
    }

    // catch it's full
    ASSERT_EQUAL(EPROCLIMIT, create_test_proc());

    reset_pcb_table();
}

/**
 * Tests add/remove pcb from queue, to be verified by user.
 */
static void test_change_queue(void) {
    kprintf("Starting test_change_queue\n");

    proc_ctrl_block_t *procs[3];

    for (int i = 0; i < 3; i++) {
        procs[i] = pid_to_proc(create_test_proc());
        ASSERT(procs[i]->pid >= 1);
    }

    print_pcb_queue(PROC_STATE_READY);
    BUSYWAIT();

    // place pcbs in reverse order
    for (int i = 2; i >= 0; i--) {
        remove_pcb_from_queue(procs[i]);
        procs[i]->curr_state = PROC_STATE_BLOCKED;
        add_pcb_to_queue(procs[i], PROC_STATE_READY);
    }

    kprintf("\n");
    print_pcb_queue(PROC_STATE_READY);

    // Remove and place back again
    for (int i = 0; i < 3; i++) {
        remove_pcb_from_queue(procs[i]);
        procs[i]->curr_state = PROC_STATE_BLOCKED;
        add_pcb_to_queue(procs[i], PROC_STATE_READY);
    }

    kprintf("\n");
    print_pcb_queue(PROC_STATE_READY);

    reset_pcb_table();
}

/**
 * Tests get_next_proc() uses FIFO queue
 */
static void test_get_next_proc(void) {
    for (int i = 0; i < 3; i++) {
        ASSERT(create_test_proc() >= 1);
    }

    print_pcb_queue(PROC_STATE_READY);
    BUSYWAIT();

    proc_ctrl_block_t *curr;

    int old_pid = 0;

    for (int i = 1; i <= 3; i++) {
        curr = get_next_proc();
        ASSERT(curr->pid > old_pid);
        old_pid = curr->pid;
        add_pcb_to_queue(curr, PROC_STATE_STOPPED);
    }

    print_pcb_queue(PROC_STATE_READY);
    BUSYWAIT();
    print_pcb_queue(PROC_STATE_STOPPED);
    BUSYWAIT();

    reset_pcb_table();
}

/**
 * Dummy func, for use by create(). Should not be entered.
 */
static void dummy(void) {
    ASSERT(0);
}

/**
 * creates a process for test purposes.
 * Also tests create(), get_next_availabe_pcb().
 */
static int create_test_proc(void) {
    return create(&dummy, DEFAULT_STACK_SIZE);
}

/**
 * Resets queue to original state. Also serves to test cleanup_proc()
 */
static void cleanup_queue(proc_state_enum_t queue) {
    proc_ctrl_block_t *curr = g_proc_queue_heads[queue];

    while(curr != NULL) {
        remove_pcb_from_queue(curr);
        cleanup_proc(curr);
        curr = g_proc_queue_heads[queue];
    }
}

/**
 * Resets pcb table to its state after dispinit()
 */
static void reset_pcb_table(void) {
    cleanup_queue(PROC_STATE_READY);
}
