/* create.c : creates and prepares a new process, and initializes the idle proc

Called from outside:
    create() - creates a new process, pushes it onto the ready queue

    init_idle_proc() - initializes idle_proc, which runs if no procs are ready
*/

#include <xeroslib.h>
#include <xeroskernel.h>
#include <pcb.h>

#define STARTING_EFLAGS 0x00003000
#define ARM_INTERRUPTS 0x00000200
#define IDLE_PROC_STACK_SIZE 1024

static void idleproc(void);
static void setup_context_frame(context_frame_t *cf, funcptr func);

/**
 * Creates new process,
 * pushes it onto the Ready queue.
 * @param func - start of the process code
 * @param stack - bytes to allocate for process's stack
 */
int create(funcptr func, int stack) {
    funcptr *fake_return_addr;
    
    if (func == NULL) {
        return EINVAL;
    }

    // if the stack is too small, make it larger
    if (stack < DEFAULT_STACK_SIZE) {
        stack = DEFAULT_STACK_SIZE;
    }

    void *stack_bottom = kmalloc(stack);
    if (stack_bottom == NULL) {
        return ENOMEM;
    }

    // run this last as it does a lot of setup, so it's harder to cleanup
    // should a failure occur further down
    proc_ctrl_block_t *new_proc = get_next_available_pcb();
    if (new_proc == NULL) {
        DEBUG("Could not find a pcb!\n");
        kfree(stack_bottom);
        return EPROCLIMIT;
    }

    new_proc->cpu_time = 0;

    // Initialize msg queues
    new_proc->blocker = NULL;
    new_proc->blocker_queue = NO_BLOCKER;
    for (int i = 0; i < 2; i++) {
        new_proc->msg_queue_heads[i] = NULL;
        new_proc->msg_queue_tails[i] = NULL;
    }

    new_proc->memory_region = stack_bottom;
    
    // place address of sysstop() as the fake return address
    fake_return_addr =
        (funcptr*)((int)stack_bottom + stack - sizeof(*fake_return_addr));
    *fake_return_addr = &sysstop;
    
    // place context, esp at high address
    new_proc->esp = (void *)((int)fake_return_addr - sizeof(context_frame_t));
    context_frame_t *new_context = new_proc->esp;
    setup_context_frame(new_context, func);

    add_pcb_to_queue(new_proc, PROC_STATE_READY);
    return new_proc->pid;
}

/**
 * Initializes the idle process.
 * @param idle_proc - pcb for the idle process
 */
void init_idle_proc(proc_ctrl_block_t* idle_proc) {
    // Since the idle proc never returns,
    // and does not interact with the outside world, we can minimize its setup
    memset(idle_proc, 0, sizeof(proc_ctrl_block_t));

    // there's no need to give the idle proc a typically sized stack
    void *stack_bottom = kmalloc(IDLE_PROC_STACK_SIZE);
    ASSERT(stack_bottom != NULL);

    idle_proc->memory_region = stack_bottom;

    // place context, esp at high address
    // don't waste space with a return address, idle_proc will never return
    idle_proc->esp = (void *)
        ((int)stack_bottom + IDLE_PROC_STACK_SIZE - sizeof(context_frame_t));
    context_frame_t *new_context = idle_proc->esp;

    setup_context_frame(new_context, &idleproc);

    // PID 0 is reserved for special/system processes
    idle_proc->pid = 0;

    // consider the idle process blocked by all other ready processes
    idle_proc->curr_state = PROC_STATE_BLOCKED;
}

/**
 * The idle process. This should only run if we are deadlocked,
 * or waiting on a hardware interrupt.
 */
static void idleproc(void) {
    while(1) {
        __asm__ volatile("hlt");
    }
    ASSERT(0);
}

/**
 * Sets a newly created process's context up.
 * Assumes the context frame has already been placed on the process's stack.
 * @param cf - context frame of the newly created process
 * @param func - start of the process code
 */
static void setup_context_frame(context_frame_t *cf, funcptr func) {
    // set all registers to 0xA5, to aid in debugging. No functionality effect.
    memset(cf, 0xA5, sizeof(context_frame_t));

    cf->iret_eip = (unsigned long)func;
    cf->iret_cs = getCS();
    cf->eflags = STARTING_EFLAGS | ARM_INTERRUPTS;

    cf->esp = ((unsigned long)cf) + 1;
    cf->ebp = cf->esp;
}
