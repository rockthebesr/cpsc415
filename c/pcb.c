/* pcb.c : process control blocks

Accessible through pcb.h:
  pcb_table_init() - initializes process queues, process control block table

  get_next_available_pcb() - returns and setups up unused pcb

  add_pcb_to_queue() - adds pcb to a queue, changes it to the appropriate state
  remove_pcb_from_queue() - remove pcb from its queue, does not change state
  add_proc_to_blocking_queue() - adds proc to another's message queue
  remove_proc_from_blocking_queue() - removes proc from another's message queue
  get_next_proc() - pops the first pcb in the READY queue, sets it to RUNNING

  cleanup_proc() - frees a pcb's contents, and prepares pcb for future use

  pid_to_proc() - returns the proc with the pid, null otherwise
  get_idleproc() - returns the idle proc

  get_all_proc_info() - fills a list of all procs's pids, statuses, and cpuTimes
  set_proc_signal() - marks a signal for delivery
  call_highest_priority_signal() - delivers highest priority signal to process

  print_pcb_queue() - prints all blocks in particular queue, testing only

Note:
  There is only 1 ready and 1 stopped queue, but there are many blocked queues.
  When blocked, a process is waiting for a particular event, like a send or
  receive. Each event emitter(in the case of send/recv, that particular proc),
  manages their own blocked queues. This way, when an event occurs, we can
  quickly find anyone waiting on the event, and address it appropriately.

Further details can be found in the documentation above the function headers.
*/

#include <xeroslib.h>
#include <xeroskernel.h>
#include <pcb.h>

// 2 queues: for processes in READY, STOPPED states
#define NUM_G_PROC_QUEUES 2
proc_ctrl_block_t *g_proc_queue_heads[NUM_G_PROC_QUEUES];
proc_ctrl_block_t *g_proc_queue_tails[NUM_G_PROC_QUEUES];
proc_ctrl_block_t g_pcb_table[PCB_TABLE_SIZE];
proc_ctrl_block_t g_idle_proc;

static void verify_pcb_queues(void);
static void fill_proc_info(processStatuses *ps, int slot,
                           proc_ctrl_block_t *proc);

static void add_proc_to_queue(proc_ctrl_block_t *proc,
                              proc_ctrl_block_t **head,
                              proc_ctrl_block_t **tail);

static void remove_proc_from_queue(proc_ctrl_block_t *proc,
                                  proc_ctrl_block_t **head,
                                  proc_ctrl_block_t **tail);

static void fail_blocked_procs(proc_ctrl_block_t *proc,
                               blocking_queue_t queue);

static void resolve_blocking(proc_ctrl_block_t *proc);

/**
 * Initializes process queues, process control block table
 */
void pcb_table_init(void) {
    // assumed throughout
    ASSERT(sizeof(int) * 8 == SIGNAL_TABLE_SIZE);

    // queues for ready and stopped processes
    for (int i = 0; i < NUM_G_PROC_QUEUES; i++) {
        g_proc_queue_heads[i] = NULL;
        g_proc_queue_tails[i] = NULL;
    }

    // stopped process queue used to find available pcbs easily
    for (int i = 0; i < PCB_TABLE_SIZE; i++) {
        // You must set the PID before adding it to the table
        // PID 0 is reserved for idleproc, and cannot be added to any queue
        g_pcb_table[i].pid = i + 1;
        add_pcb_to_queue(&g_pcb_table[i], PROC_STATE_STOPPED);
    }

    init_idle_proc(&g_idle_proc);
}

/**
 * Removes the next process from the ready queue,
 * and returns it as a PROC_STATE_RUNNING proc
 * @return: next PCB to be run, set to PROC_STATE_RUNNING
 */
proc_ctrl_block_t* get_next_proc(void) {
    proc_ctrl_block_t *proc = g_proc_queue_heads[PROC_STATE_READY];

    if (proc != NULL) {
        remove_pcb_from_queue(proc);
    } else {
        proc = &g_idle_proc;
    }

    proc->curr_state = PROC_STATE_RUNNING;
    return proc;
}

/**
 * Gets a free pcb in the pcb_table, assigns it a pid, and peforms basic setup.
 * @return the free pcb, or NULL if none are free.
 */
proc_ctrl_block_t* get_next_available_pcb(void) {
    // STOPPED queue contains pcbs which are no longer needed,
    // allows us to find a free pcb in constant time.
    proc_ctrl_block_t *proc = g_proc_queue_heads[PROC_STATE_STOPPED];
    if (proc == NULL) {
        DEBUG("PCB table is full!\n");
        return NULL;
    }

    remove_pcb_from_queue(proc);

    // save old pid before clearing proc, it is used to calculate the new pid
    int old_pid = proc->pid;

    // many pcb fields are initialized to 0,
    // and this also prevents values from leaking
    memset(proc, 0, sizeof(proc_ctrl_block_t));

    proc->signal_table = kmalloc(SIGNAL_TABLE_SIZE * sizeof(funcptr_args1));
    if (proc->signal_table == NULL) {
        proc->pid = old_pid;
        add_pcb_to_queue(proc, PROC_STATE_STOPPED);
        return NULL;
    }

    memset(proc->signal_table, 0,
           sizeof(SIGNAL_TABLE_SIZE * sizeof(funcptr_args1)));

    proc->signals_enabled = 1;

    proc->curr_state = PROC_STATE_STOPPED;
    proc->blocking_queue_name = NO_BLOCKER;

    // To allow us to access a proc by its pid in constant time,
    // we set our pids a multiple of PCB_TABLE_SIZE
    proc->pid = old_pid + PCB_TABLE_SIZE;

    // by the time we've overflowed, its probably fine to reuse a pid
    if (proc->pid < 1) {
        proc->pid = old_pid % PCB_TABLE_SIZE;
    }

    ASSERT(proc->pid >= 1);

    return proc;
}

/**
 * Returns the proc if it exists, null otherwise. Can be used to validate pids.
 * @param pid - the process to check
 * @return the proc's pcb
 */
proc_ctrl_block_t* pid_to_proc(int pid) {
    if (pid >= 1) {
        proc_ctrl_block_t *proc = &g_pcb_table[(pid - 1) % PCB_TABLE_SIZE];
        if (proc->pid == pid && proc->curr_state != PROC_STATE_STOPPED) {
            return proc;
        }
    }

    return NULL;
}

/**
 * fills a list of all procs's pids, statuses, and cpuTimes
 * @param ps - process status block to contain data
 * @return index of final slot filled
 */
int get_all_proc_info(processStatuses *ps) {
    ASSERT(ps != NULL);

    int slot = 0;

    // idleproc is always added
    proc_ctrl_block_t* proc = get_idleproc();
    fill_proc_info(ps, slot, proc);

    for (int i = 0; i < PCB_TABLE_SIZE; i++) {
        proc = &g_pcb_table[i];
        if (proc->curr_state != PROC_STATE_STOPPED) {
            slot++;
            fill_proc_info(ps, slot, proc);
        }
    }

    return slot;
}

/**
 * Marks a signal for delivery
 * @param proc - the process to deliver the signal to
 * @param signal - the signal to deliver
 * @return 0 on success, error code on failure
 */
int set_proc_signal(proc_ctrl_block_t *proc, int signal) {
    ASSERT(proc != NULL);

    if (signal < 0 || signal >= SIGNAL_TABLE_SIZE) {
        return SYSKILL_INVALID_SIGNAL;
    }

    // NULL signal handler indicates to ignore signal
    if (proc->signal_table[signal]) {
        proc->signals_fired |= (1 << signal);

        if (proc->curr_state == PROC_STATE_BLOCKED) {
            resolve_blocking(proc);
        }
    }

    return 0;
}

/**
 * delivers highest priority signal to process
 * @param proc - process to deliver a waiting signal to
 */
void call_highest_priority_signal(proc_ctrl_block_t *proc) {
    ASSERT(proc->signals_fired != 0);

    // find highest priority signal. Higher place in table -> higher priority
    int signal_num = SIGNAL_TABLE_SIZE - 1;
    int signal_bit = 1 << signal_num;
    while ((proc->signals_fired & signal_bit) == 0) {
        signal_num--;
        signal_bit = signal_bit >> 1;
    }

    // clear signal
    proc->signals_fired &= ~signal_bit;

    signal(proc->pid, signal_num);
}

/**
 * fills a single entry of processStatuses
 * @param ps - a processStatuses struct
 * @param slot - the entry in each of ps's arrays to fill
 * @param proc - the process to extract information from
 */
static void fill_proc_info(processStatuses *ps, int slot,
                           proc_ctrl_block_t *proc) {
    ASSERT(ps != NULL && proc != NULL);
    ASSERT(0 <= slot && slot < PCB_TABLE_SIZE);

    ps->pid[slot] = proc->pid;
    ps->status[slot] = proc->curr_state;
    ps->cpuTime[slot] = proc->cpu_time * TICK_LENGTH_IN_MS;
}

/**
 * Frees allocated memory associated with proc,
 * and places the pcb in the STOPPED queue.
 * Assumes proc is not in any queue.
 * @param proc - the process control block to cleanup
 */
void cleanup_proc(proc_ctrl_block_t *proc) {
    ASSERT(proc != NULL);
    // memory_region and esp point to two ends of the same block.
    // memory_region is returned by kmalloc.
    // Because the stack grows down, we set esp to the end of this block.
    // Therefore, in cleanup, we do not free esp, only memory_region
    kfree(proc->memory_region);

    kfree(proc->signal_table);

    // all blocked procs on the msg queues must be notified
    fail_blocked_procs(proc, SENDER);
    fail_blocked_procs(proc, RECEIVER);
    fail_blocked_procs(proc, WAITING);

    if (proc->blocking_queue_name != NO_BLOCKER) {
        resolve_blocking(proc);
    }

    add_pcb_to_queue(proc, PROC_STATE_STOPPED);
}

/**
 * Handle's a proc's premature end to being unblocked
 * @param proc - the process to unblock
 */
static void resolve_blocking(proc_ctrl_block_t *proc) {
    ASSERT(proc != NULL && proc->blocking_queue_name != NO_BLOCKER);
    ASSERT_EQUAL(proc->curr_state, PROC_STATE_BLOCKED);

    if (proc->blocking_queue_name == SLEEP) {
        wake(proc);
    } else if (proc->blocking_proc && proc->blocking_proc != proc) {
        // proc->blocking_proc == proc indicates proc called recv_any
        int in_queue = remove_proc_from_blocking_queue(proc, proc->blocking_proc,
                                                 proc->blocking_queue_name);
        ASSERT_EQUAL(in_queue, 1);
        proc->ret = 0;
    }
}

/**
 * Alerts all processes in a proc's message queue that their call has failed,
 * as the target proc is being killed.
 * @param proc - the owner of the message queue
 * @param queue - the particular message queue to flush
 */
static void fail_blocked_procs(proc_ctrl_block_t *proc,
                                   blocking_queue_t queue) {
    ASSERT(proc != NULL);
    int ret;
    proc_ctrl_block_t *curr = proc->blocking_queue_heads[queue];

    while (curr != NULL) {
        ret = remove_proc_from_blocking_queue(curr, proc, queue);
        ASSERT_EQUAL(ret, 1);

        curr->ret = SYSPID_DNE;
        add_pcb_to_queue(curr, PROC_STATE_READY);

        curr = proc->blocking_queue_heads[queue];
    }
}

/**
 * Adds pcb to a new queue,
 * change its state to new_state.
 * @param proc - the process control block to add to the queue
 * @param new_state - the new state of the process
 */
void add_pcb_to_queue(proc_ctrl_block_t *proc, proc_state_enum_t new_state) {
    ASSERT(proc != NULL);
    ASSERT(proc->curr_state != new_state);
    ASSERT(new_state < NUM_G_PROC_QUEUES && new_state >= 0);

    proc->curr_state = new_state;

    if (proc->pid == 0) {
        // Do not add the idle proc to the queue
        return;
    }

    add_proc_to_queue(proc, &g_proc_queue_heads[new_state],
                      &g_proc_queue_tails[new_state]);

    verify_pcb_queues();
}

/**
 * Removes proc from its current queue,
 * does not change its state.
 * @param proc - the process control block to remove
 */
void remove_pcb_from_queue(proc_ctrl_block_t *proc) {
    ASSERT(proc != NULL);
    ASSERT(proc->curr_state != PROC_STATE_RUNNING);
    ASSERT(proc->curr_state < NUM_G_PROC_QUEUES && proc->curr_state >= 0);

    proc_state_enum_t state = proc->curr_state;
    remove_proc_from_queue(proc, &g_proc_queue_heads[state],
                           &g_proc_queue_tails[state]);

    verify_pcb_queues();
}

/**
 * Adds a proc to another's message queue
 * @param proc - proc to add
 * @param queue_owner - whose message queue to alter
 * @param queue - indicates sending or receiving queue
 */
void add_proc_to_blocking_queue(proc_ctrl_block_t *proc,
                          proc_ctrl_block_t *queue_owner,
                          blocking_queue_t queue) {

    ASSERT(proc != NULL && queue_owner != NULL);
    add_proc_to_queue(proc, &queue_owner->blocking_queue_heads[queue],
                      &queue_owner->blocking_queue_tails[queue]);

    // A proc can only be blocked on one item at a time.
    // We keep track of which queue we're in, so we can
    // remove ourselves, should we be killed.
    proc->blocking_proc = queue_owner;
    proc->blocking_queue_name = queue;
}

/**
 * Removes a proc from a message queue
 * @param proc - proc to remove
 * @param queue_owner - whose message queue to alter
 * @param queue - indicates sending or receiving queue
 * @return 1 if proc was in queue, 0 otherwise
 */
int remove_proc_from_blocking_queue(proc_ctrl_block_t *proc,
                              proc_ctrl_block_t *queue_owner,
                              blocking_queue_t queue) {

    ASSERT(proc != NULL && queue_owner != NULL);

    // check if proc is actually on queue_owner's specific blocking_queue
    if (proc->blocking_queue_name != queue ||
        proc->blocking_proc != queue_owner) {
        return 0;
    }

    ASSERT_EQUAL(proc->curr_state, PROC_STATE_BLOCKED);
    remove_proc_from_queue(proc, &queue_owner->blocking_queue_heads[queue],
                           &queue_owner->blocking_queue_tails[queue]);

    proc->blocking_proc = NULL;
    proc->blocking_queue_name = NO_BLOCKER;

    return 1;
}

/**
 * Adds a proc to the tail of any queue composed of pcbs
 * @param proc - proc to add
 * @param head - head of the queue
 * @param tail - tail of the queue
 */
static void add_proc_to_queue(proc_ctrl_block_t *proc,
                              proc_ctrl_block_t **head,
                              proc_ctrl_block_t **tail) {
    ASSERT(proc != NULL && head != NULL && tail != NULL);

    if (*head == NULL) {
        // List is empty, update head as well
        *head = proc;
    } else {
        // List is not empty, update tail
        (*tail)->next_proc = proc;
    }
    proc->prev_proc = *tail;
    proc->next_proc = NULL;
    *tail = proc;
}

/**
 * Removes a proc from any queue composed of pcbs. Assumes we are in the queue.
 * @param proc - proc to remove
 * @param head - head of the queue
 * @param tail - tail of the queue
 */
static void remove_proc_from_queue(proc_ctrl_block_t *proc,
                                  proc_ctrl_block_t **head,
                                  proc_ctrl_block_t **tail) {
    ASSERT(proc != NULL && head != NULL && tail != NULL);

    if (proc->prev_proc) {
        proc->prev_proc->next_proc = proc->next_proc;
    }

    if (proc->next_proc) {
        proc->next_proc->prev_proc = proc->prev_proc;
    }

    if (*head == proc) {
        *head = proc->next_proc;
    }

    if (*tail == proc) {
        *tail = proc->prev_proc;
    }

    // done for safety
    proc->prev_proc = NULL;
    proc->next_proc = NULL;
}

/**
 * gets the idle proc's pcb
 * @return pointer to the idle proc
 */
proc_ctrl_block_t* get_idleproc(void) {
    return &g_idle_proc;
}

/**
 * Debugging function to help print queue contents
 * @param queue: the queue to dump
 */
void print_pcb_queue(proc_state_enum_t queue) {
    ASSERT(queue != PROC_STATE_RUNNING);
    ASSERT(queue != PROC_STATE_BLOCKED);
    proc_ctrl_block_t *curr = g_proc_queue_heads[queue];

    int count = 0;

    DEBUG("Queue %d: ", queue);
    while(curr != NULL) {
        kprintf("{PID: %d, state: %d}", curr->pid, curr->curr_state);
        curr = curr->next_proc;
        count++;
    }
    kprintf("\n");

    if (count == 0) {
        DEBUG("Queue %d is empty\n", queue);
    } else {
        DEBUG("Total items: %d\n", count);
    }
}

/**
 * Debugging function to sanity check all our PCB queues
 */
static void verify_pcb_queues(void) {
    proc_ctrl_block_t *curr;
    for (int i = 0; i < NUM_G_PROC_QUEUES; i++) {
        curr = g_proc_queue_heads[i];
        if (curr != NULL) {
            ASSERT_EQUAL(curr->prev_proc, NULL);
            ASSERT_EQUAL(curr->curr_state, i);

            if (curr->next_proc) {
                ASSERT_EQUAL(curr->next_proc->prev_proc, curr);
                curr = curr->next_proc;
            }

            while(curr->next_proc) {
                ASSERT_EQUAL(curr->curr_state, i);
                ASSERT_EQUAL(curr->prev_proc->next_proc, curr);
                ASSERT_EQUAL(curr->next_proc->prev_proc, curr);
                curr = curr->next_proc;
            }

            if (curr->prev_proc) {
                ASSERT_EQUAL(curr->prev_proc->next_proc, curr);
            }
            ASSERT_EQUAL(curr->curr_state, i);
            ASSERT_EQUAL(curr->next_proc, NULL);
        }
    }
}
