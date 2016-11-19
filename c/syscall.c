/* syscall.c : syscalls

Called from user processes:
    syscreate() - create a new process
    sysyield() - pause execution and allow another process to run
    sysstop() - stops process
    sysgetpid() - returns current process's pid
    syskill() - kills identified process
    syssend() - sends data to a particular process
    sysrecv() - receives data delivered by syssend()
    syssendbuf() - generalized sysend, for sending > 4 bytes
    sysrecvbuf() - generalized sysrecv, for receiving > 4 bytes
    syssleep() - allows process to sleep for a number of milliseconds
    sysgetcputime() - gets number of quantums the process has consumed
    syssighandler() - registers the handler as a signal handler
    syssigreturn() - restores a process's context after a signal is handled

Helper functions:
    syscallX - prepare stack for syscall with X parameters
                and signal interrupt
 */

#include <xeroskernel.h>

static unsigned long request_reg;
static int syscall0(int request);
static int syscall1(int request, unsigned long arg1);
static int syscall2(int request, unsigned long arg1, unsigned long arg2);
static int syscall3(int request, unsigned long arg1,
                    unsigned long arg2, unsigned long arg3);

/**
 * Create a new process
 * @param func - main function of new process
 * @param stack - size of process's stack
 */
unsigned int syscreate(funcptr func, int stack) {
    return (unsigned int)syscall2(SYSCALL_CREATE,
                                  (unsigned long)func, (unsigned long)stack);
}

/**
 * Pause the execution of this process and allow another process to run
 */
void sysyield(void) {
    syscall0(SYSCALL_YIELD);
}

/**
 * Stops this process
 */
void sysstop(void) {
    syscall0(SYSCALL_STOP);
}

/**
 * Gets the current proc's pid
 * @return - current proc's pid
 */
int sysgetpid(void) {
    return syscall0(SYSCALL_GETPID);
}

/**
 * Terminates identified process
 * @param pid - pid of the process to kill
 * @return 0 on success, -1 if process does not exist, -2 if pid is its own
 */
int syskill(int pid) {
    return syscall1(SYSCALL_KILL, (unsigned long)pid);
}

/**
 * To be used by processes to perform synchronized output to the screen
 * @param str - null terminated string to be printed
 */
void sysputs(char *str) {
    syscall1(SYSCALL_PUTS, (unsigned long)str);
}

/**
 * Delivers data from this process to another. That process must call sysrecv()
 * @param dest_pid - pid of process to send to
 * @param buffer - buffer containing data
 * @param len - length of buffer
 * @return 0 on success, -1 if proc terminates or does not exist,
 *         -2 if sending to itself, -3 if any other error.
 */
int syssendbuf(int dest_pid, void *buffer, unsigned long len) {
    //TODO: should we implement this further,
    // we either return bytes sent, or len is changed to a ptr and does this
    return syscall3(SYSCALL_SEND, (unsigned long)dest_pid,
                    (unsigned long)buffer, len);
}

/**
 * Called to receive data from a process which has called, or will call, syssend()
 * @param from_pid - pid of proc to receive from. Put 0 to receive from any proc
 *                   (*from_pid will be modified to the PID of the sending proc)
 * @param buffer - buffer to store received data
 * @param len - length of buffer
 * @return 0 on success, -1 if proc terminates or does not exist,
 *         -2 if sending to itself, -3 if any other error.
 */
int sysrecvbuf(int *from_pid, void *buffer, unsigned long len) {
    //TODO: should we implement this further,
    // len should be a ptr and set to bytes received
    return syscall3(SYSCALL_RECV, (unsigned long)from_pid,
                    (unsigned long)buffer, len);
}

/**
 * Delivers data from this process to another. That process must call sysrecv()
 *
 * Note that this function is simply a more general wrapper around syssendbuf().
 * This was done in anticipation of our kernel needing block data communication,
 * and it would have been roughly the same amount of work to implement that
 * as it would to implement a single unsigned long amount of data.
 *
 * @param dest_pid - pid of process to send to
 * @param num - the data to deliver
 * @return 0 on success, -1 if proc terminates or does not exist,
 *         -2 if sending to itself, -3 if any other error.
 */
int syssend(int dest_pid, unsigned long num) {
    return syssendbuf(dest_pid, &num, sizeof(num));
}

/**
 * Called to receive a single unsigned long from another process.
 *
 * Note that this function is simply a more general wrapper around sysrecvbuf().
 * This was done in anticipation of our kernel needing block data communication,
 * and it would have been roughly the same amount of work to implement that
 * as it would to implement a single unsigned long amount of data.
 *
 * @param from_pid - pid of process to receive from. If 0, recv_any is called,
 *                   and we update from_pid to the value of the sending pid
 * @param num - buffer to store the received data
 * @return 0 on success, -1 if pid is invalid,
 *         -2 if receiving from itself, -3 if any other error.
 */
int sysrecv(int *from_pid, unsigned long *num) {
    return sysrecvbuf(from_pid, (void*)num, sizeof(num));
}

/**
 * Allows process to sleep for a number of milliseconds
 * @param milliseconds - the time to sleep for
 * @return abs((time requested to sleep for) - (time actually slept))
 */
unsigned int syssleep(unsigned int milliseconds) {
    return syscall1(SYSCALL_SLEEP, milliseconds);
}

/**
 * Returns the number of quantums the process has consumed
 * @param pid - pid of proc to check. If -1, currproc is checked, if 0, idleproc
 * @return number of ticks consumed, or -1 if pid does not exist
 */
int sysgetcputime(int pid) {
    return syscall1(SYSCALL_CPUTIME, pid);
}

/**
 * Registers the provided function as the handler for the signal.
 * Returns the old handler for the signal through the provided pointer.
 * @param signal - the signal to register the handler for
 * @param newhandler - the new handler for the signal
 * @param oldHandler
 * @return 0 on success, -1 if signal is invalid, -2 if the funcptrs are invalid
 */
int syssighandler(int signal, funcptr_args1 newhandler,
                  funcptr_args1 *oldHandler) {
    return syscall3(SYSCALL_SIGHANDLER, signal, (unsigned long)newhandler,
                    (unsigned long)oldHandler);
}

/**
 * Restore's a process after it has finished handling a signal.
 * Used only by signal trampoline code. Does not return.
 * @param old_sp - the stack pointer when the signal occured
 */
void syssigreturn(void *old_sp) {
    syscall1(SYSCALL_SIGRETURN, (unsigned long)old_sp);
    ASSERT(0);
}


/*****************************************************************************
 * general syscallX functions which prepares the stack for a syscall
 *
 * In all cases, all parameters (REQ_ID and arg1, arg2, ..., argN) are all
 * pushed onto the stack for the kernel. When the kernel completes, we need
 * to pop all of these parameters off the stack so that our stack is back the
 * way it used to be.
 *
 * The syscall return value is stored in %eax. It is moved into request_reg and
 * returned from the syscall.
 *****************************************************************************/
 
static int syscall0(int request) {
    __asm__ volatile( " \
        push 8(%%ebp) \n\
        int $50 \n\
        pop request_reg \n\
        movl %%eax, request_reg \n\
    "
    : /* no outputs */
    : /* no range */
    : "%eax"
    );
    
    return (int)request_reg;
}

static int syscall1(int request, unsigned long arg1) {
    __asm__ volatile( " \
        push 12(%%ebp) \n\
        push 8(%%ebp) \n\
        int $50 \n\
        pop request_reg \n\
        pop request_reg \n\
        movl %%eax, request_reg \n\
    "
    : /* no outputs */
    : /* no range */
    : "%eax"
    );

    return (int)request_reg;
}

static int syscall2(int request, unsigned long arg1, unsigned long arg2) {
    __asm__ volatile( " \
        push 16(%%ebp) \n\
        push 12(%%ebp) \n\
        push 8(%%ebp) \n\
        int $50 \n\
        pop request_reg \n\
        pop request_reg \n\
        pop request_reg \n\
        movl %%eax, request_reg \n\
    "
    : /* no outputs */
    : /* no range */
    : "%eax"
    );
    
    return (int)request_reg;
}

static int syscall3(int request, unsigned long arg1,
                    unsigned long arg2, unsigned long arg3) {
    __asm__ volatile( " \
        push 20(%%ebp) \n\
        push 16(%%ebp) \n\
        push 12(%%ebp) \n\
        push 8(%%ebp) \n\
        int $50 \n\
        pop request_reg \n\
        pop request_reg \n\
        pop request_reg \n\
        pop request_reg \n\
        movl %%eax, request_reg \n\
    "
    : /* no outputs */
    : /* no range */
    : "%eax"
    );
    
    return (int)request_reg;
}
