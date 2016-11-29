/* signal.c : support for signal handling

Called from outside:
    sigtramp() - executed by process in user space to handle a signal
    signal() - sets up process stack to execute signal handler via sigtramp

Further details can be found in the documentation above the function headers.
*/

#include <xeroskernel.h>
#include <xeroslib.h>
#include <pcb.h>

/**
 * Executed by a process in user space to handle a signal
 * @param handler - proc's signal handler
 * @param cntx - process context at moment of signal
 */
void sigtramp(funcptr_args1 handler, void* cntx) {
    handler(cntx);
    syssigreturn(cntx);
}

/**
 * Sets up a process's stack to execute signal handler via sigtramp
 * @param pid - pid of the proc to signal
 * @param sig_no - signal number of signal to fire
 * @return 0 on success, error code on failure
 */
int signal(int pid, int sig_no) {
    proc_ctrl_block_t* proc = pid_to_proc(pid);
    if (proc == NULL) {
        return SYSPID_DNE;
    }

    if (sig_no < 0 || sig_no >= SIGNAL_TABLE_SIZE) {
        return SIGNAL_DNE;
    }

    proc->signals_enabled = 0;

    int* stack_ptr = (int*)proc->esp;

    // save return value
    stack_ptr = (int*)(((int)stack_ptr) - sizeof(int));
    *stack_ptr = proc->ret;

    // push current context, 2nd argument of sigtramp, onto stack
    stack_ptr -= 1;
    *stack_ptr = (int)proc->esp;

    // push handler onto stack
    stack_ptr -= 1;
    *stack_ptr = (int)proc->signal_table[sig_no];

    // push dummy return address
    stack_ptr -= 1;
    *stack_ptr = 0xCAFECAFE;

    proc->esp = (void *)((int)stack_ptr - sizeof(context_frame_t));
    context_frame_t *new_context = proc->esp;
    setup_context_frame(new_context, (funcptr)(&sigtramp));

    return 0;
}
