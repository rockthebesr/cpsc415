/* sleep.c : sleep device

Called from outside:
    sleep() - Puts a process to sleep for a specific amount of time
    wake() - Ends the sleep of a process

    tick() - Monitors time, so we know when sleeping procs are done.

Note:
  To monitor time efficiently in our sleeping queue, we utilize a delta list.
  For convenience, time left is stored in proc->ret. This allows us to expand
  our functionality, while fitting nicely into the current pcb workflow.
  Additionally, when a process is removed from the list, we have a count of how
  many time slices were remaining in its sleep.

Further details can be found in the documentation above the function headers.
*/

#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

// sleeping procs, maintained as a delta-list
proc_ctrl_block_t *g_sleeping_list = NULL;

static void add_to_sleeping_list(proc_ctrl_block_t *proc);
static void remove_from_sleeping_list(proc_ctrl_block_t *proc);

/**
 * Puts a process to sleep for a specific amount of time.
 * A process is considered blocked while it is sleeping.
 * @param proc - the process to sleep
 * @param time - the time, in milliseconds, for the process to sleep
 */
void sleep(proc_ctrl_block_t *proc, unsigned int time) {
    ASSERT(time > 0);
    proc->curr_state = PROC_STATE_BLOCKED;
    proc->blocking_queue_name = SLEEP;
    proc->blocking_proc = NULL;

    proc->ret = time / TICK_LENGTH_IN_MS + (time % TICK_LENGTH_IN_MS ? 1 : 0);
    add_to_sleeping_list(proc);
}

/**
 * Ends the sleep of a process
 * Assumes the process is in the sleeping queue
 * @param proc - the process to wake
 */
void wake(proc_ctrl_block_t *proc) {
    ASSERT(proc != NULL);
    ASSERT_EQUAL(proc->blocking_proc, NULL);
    ASSERT_EQUAL(proc->blocking_queue_name, SLEEP);

    remove_from_sleeping_list(proc);
    proc->blocking_queue_name = NO_BLOCKER;

    proc->ret *= TICK_LENGTH_IN_MS;
}

/**
 * Called at the end of a time slice.
 * Decreases the time each sleeping proc is waiting,
 * removes those that finish, places them on the ready queue
 */
void tick(void) {
    if (g_sleeping_list == NULL) {
        return;
    }

    proc_ctrl_block_t *proc;

    // subtract 1 time slice, and remove all expired procs
    g_sleeping_list->ret--;
    while (g_sleeping_list != NULL && g_sleeping_list->ret <= 0) {
        // wake may change the value of g_sleeping_list
        proc = g_sleeping_list;
        wake(proc);
        add_pcb_to_queue(proc, PROC_STATE_READY);
    }
}

/**
 * Adds the proc to the global sleeping list priority queue
 * @param proc - the process to add
 */
static void add_to_sleeping_list(proc_ctrl_block_t *proc) {
    ASSERT(proc != NULL);
    proc_ctrl_block_t *prev = NULL;
    proc_ctrl_block_t *entry = g_sleeping_list;
    
    while (entry != NULL && proc->ret > entry->ret) {
        proc->ret -= entry->ret;
        prev = entry;
        entry = entry->next_proc;
    }

    if (prev == NULL) {
        g_sleeping_list = proc;
    } else {
        prev->next_proc = proc;
    }
    
    proc->next_proc = entry;
    proc->prev_proc = prev;
    
    if (proc->next_proc != NULL) {
        proc->next_proc->prev_proc = proc;
        proc->next_proc->ret -= proc->ret;
    }
}

/**
 * Removes the process from the global sleep list.
 * Assumes that proc is in the list
 * @param proc - the process to remove
 */
static void remove_from_sleeping_list(proc_ctrl_block_t *proc) {
    ASSERT(proc != NULL && proc->curr_state == PROC_STATE_BLOCKED);

    if (proc->prev_proc) {
        proc->prev_proc->next_proc = proc->next_proc;
    }

    if (proc->next_proc) {
        proc->next_proc->ret += proc->ret;
        proc->next_proc->prev_proc = proc->prev_proc;
    }

    if (g_sleeping_list == proc) {
        g_sleeping_list = proc->next_proc;
    }

    // done for safety
    proc->prev_proc = NULL;
    proc->next_proc = NULL;
}
