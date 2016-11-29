/* ctsw.c : context switcher

Called from outside:
    ctsw_init_evec() - initializes irq handlers to enter the context switcher
    
    ctsw_contextswitch() - context switch from kernel to user process

Further details can be found in the documentation above the function headers.
*/

#include <xeroskernel.h>
#include <pcb.h>

void _timer_entry_point(void);
void _syscall_entry_point(void);
void _common_entry_point(void);
static void *kern_stack_ptr;
static unsigned long *ESP;
static unsigned long REQ_ID;
static int is_syscall;

/**
 * Sets the syscall and timer interrupt handlers
 */
void ctsw_init_evec(void) {
    set_evec(TIMER_INTERRUPT_VALUE, (unsigned long)_timer_entry_point);
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

    if (proc->signals_fired) {
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
_timer_entry_point: \n\
        cli \n\
        pusha \n\
        movl $0, is_syscall \n\
        jmp _common_entry_point \n\
_syscall_entry_point: \n\
        cli \n\
        pusha \n\
        movl $1, is_syscall \n\
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

    if (is_syscall) {
        REQ_ID = cf->syscallargs[0];
        proc->args = &(cf->syscallargs[1]);
    } else {
        REQ_ID = TIMER_INT;
        proc->args = (unsigned long *)0xDEADBEEF;
    }

    return REQ_ID;
}
