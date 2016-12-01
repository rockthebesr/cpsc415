/* ctsw.c : context switcher

Called from outside:
    ctsw_init_evec() - initializes irq handlers to enter the context switcher
    
    ctsw_contextswitch() - context switch from kernel to user process

Further details can be found in the documentation above the function headers.
*/

#include <xeroskernel.h>
#include <pcb.h>

#define CTSW_SYSCALL  0
#define CTSW_TIMER  1
#define CTSW_KEYBOARD  2

void _timer_entry_point(void);
void _keyboard_entry_point(void);
void _syscall_entry_point(void);
void _common_entry_point(void);
static void *kern_stack_ptr;
static unsigned long *ESP;
static unsigned long REQ_ID;
static int ctsw_reason;

/**
 * Sets the syscall and timer interrupt handlers
 */
void ctsw_init_evec(void) {
    set_evec(TIMER_INTERRUPT_VALUE, (unsigned long)_timer_entry_point);
    set_evec(KEYBOARD_INTERRUPT_VALUE, (unsigned long)_keyboard_entry_point);
    set_evec(SYSCALL_INTERRUPT_VALUE, (unsigned long)_syscall_entry_point);
}

/**
 * Main context switching function.
 * Switches from kernel into the user process specified by proc
 * @param proc: process to switch to
 */
syscall_request_id_t ctsw_contextswitch(proc_ctrl_block_t *proc) {
    // for getting rid of unused param compiler warning
    (void)kern_stack_ptr;

    if (proc->signals_fired && proc->signals_enabled) {
        call_highest_priority_signal(proc);
    }

    context_frame_t *cf;

    ESP = proc->esp;

    cf = (context_frame_t *)proc->esp;
    cf->eax = proc->ret;
    
    __asm__ volatile( " \
        pushf \n\
        pusha \n\
        movl %%esp, kern_stack_ptr \n\
        movl ESP, %%esp \n\
        popa \n\
        iret \n\
_syscall_entry_point: \n\
        cli \n\
        pusha \n\
        movl $0, ctsw_reason \n\
        jmp _common_entry_point \n\
_timer_entry_point: \n\
        cli \n\
        pusha \n\
        movl $1, ctsw_reason \n\
        jmp _common_entry_point \n\
_keyboard_entry_point: \n\
        cli \n\
        pusha \n\
        movl $2, ctsw_reason \n\
        jmp _common_entry_point \n\
_common_entry_point: \n\
        movl %%esp, ESP \n\
        movl kern_stack_ptr, %%esp \n\
        popa \n\
        popf \n"
    : /* no outputs */
    : /* no range */
    : "%eax"
    );
    
    proc->esp = ESP;

    cf = (context_frame_t *)proc->esp;
    proc->ret = cf->eax;

    switch (ctsw_reason) {
    case CTSW_SYSCALL:
        REQ_ID = cf->syscallargs[0];
        proc->args = &(cf->syscallargs[1]);
        break;
    case CTSW_TIMER:
        REQ_ID = TIMER_INT;
        proc->args = (unsigned long *)0xDEADBEEF;
        break;
    case CTSW_KEYBOARD:
        REQ_ID = KEYBOARD_INT;
        proc->args = (unsigned long *)0xCAFEBABE;
        break;
    default:
        kprintf("Kernel encountered unexpected ctsw_reason %d. Halting.\n", ctsw_reason);
        while(1);
    }

    return REQ_ID;
}
