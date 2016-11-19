/* disp.c : dispatcher

Called from outside:
    dispinit() - initializes dispatcher
    
    dispatch() - starts the root process
    
Further details can be found in the documentation above the function headers.
 */

#include <xeroskernel.h>
#include <pcb.h>
#include <i386.h>
#include <copyinout.h>

/* Syscall dispatches */
static void timer_handler(void);
static int dispatch_syscall_create(void);
static int dispatch_syscall_kill(void);
static void dispatch_syscall_puts(void);
static void dispatch_syscall_send(void);
static void dispatch_syscall_recv(void);
static void dispatch_syscall_sleep(void);
static int dispatch_syscall_getcputime(void);

static proc_ctrl_block_t *currproc;

/**
 * Initializes the dispatcher
 */
void dispinit(void) {
    pcb_table_init();
}

/**
 * Passes kernel control to the dispatcher. This function does not return.
 * The root process is the root_proc provided.
 *
 * The dispatcher's behaviour is undefined if the root process terminates.
 *
 * @param root_proc: root process
 */
void dispatch(funcptr root_proc) {
    create(root_proc, DEFAULT_STACK_SIZE);
    currproc = get_next_proc();

    while(1) {
        syscall_request_id_t request = ctsw_contextswitch(currproc);

        switch(request) {

        case TIMER_INT:
            timer_handler();
            break;

        case SYSCALL_CREATE:
            currproc->ret = dispatch_syscall_create();
            break;

        case SYSCALL_YIELD:
            add_pcb_to_queue(currproc, PROC_STATE_READY);
            currproc = get_next_proc();
            break;

        case SYSCALL_STOP:
            cleanup_proc(currproc);
            currproc = get_next_proc();
            break;

        case SYSCALL_GETPID:
            currproc->ret = currproc->pid;
            break;

        case SYSCALL_KILL:
            currproc->ret = dispatch_syscall_kill();
            break;

        case SYSCALL_PUTS:
            dispatch_syscall_puts();
            break;

        case SYSCALL_SEND:
            dispatch_syscall_send();
            break;

        case SYSCALL_RECV:
            dispatch_syscall_recv();
            break;

        case SYSCALL_SLEEP:
            dispatch_syscall_sleep();
            break;

        case SYSCALL_CPUTIME:
            currproc->ret = dispatch_syscall_getcputime();
            break;

        default:
            DEBUG("Unknown syscall request: %d\n", request);
            ASSERT(0);
        }
    }
}

/**
 * Handler for the syscreate syscall
 * @return On success, pid of process created. Error code on failure.
 */
static int dispatch_syscall_create(void) {
    funcptr func;
    int stack;
    int result;
    
    result = verify_usrptr((void*)currproc->args[0], sizeof(funcptr));
    if (result != OK) {
        return result;
    }
    
    func = (funcptr)currproc->args[0];
    stack = currproc->args[1];
    return create(func, stack);
}

/**
 * Handler for the syskill syscall
 * @return returns 0 on success, or error code - see syskill for details
 */
static int dispatch_syscall_kill() {
    int pid = currproc->args[0];

    // pids start at 1, cannot be negative
    if (pid < 1) {
        return SYSPID_DNE;
    }

    // process cannot kill itself - should use sysstop() instead
    if (currproc->pid == pid) {
        return SYSPID_ME;
    }

    return terminate_proc_if_exists(pid);
}

/**
 * Handler for the sysputs syscall
 */
static void dispatch_syscall_puts(void) {
    char *str = (char*)currproc->args[0];

    // check user's string starts in a valid location
    int result = verify_usrstr((void*)str);
    if (result == OK) {
        kprintf(str);
    }
}

/**
 * Handler for the syssend syscall
 */
static void dispatch_syscall_send(void) {
    int dest_pid = (int)currproc->args[0];
    void *buffer = (void*)currproc->args[1];
    unsigned long len = (unsigned long)currproc->args[2];

    if (dest_pid == currproc->pid) {
        currproc->ret = SYSPID_ME;
        return;
    }
    
    if (len <= 0 || verify_usrptr(buffer, len) != OK) {
        currproc->ret = SYSERR_OTHER;
        return;
    }

    proc_ctrl_block_t *destproc = pid_to_proc(dest_pid);
    if (destproc == NULL) {
        currproc->ret = SYSPID_DNE;
        return;
    }
    
    currproc->ret = send(currproc, destproc, buffer, len);
    if (currproc->ret == SYSMSG_BLOCKED) {
        currproc->curr_state = PROC_STATE_BLOCKED;
        currproc = get_next_proc();
    }
    
    return;
}

/**
 * Handler for the sysrecv syscall
 */
static void dispatch_syscall_recv(void) {
    int *from_pid = (int*)currproc->args[0];
    void *buffer = (void*)currproc->args[1];
    unsigned long len = (int)currproc->args[2];

    if (verify_usrptr(from_pid, sizeof(int)) != OK ||
        len <= 0 || verify_usrptr(buffer, len) != OK) {
        currproc->ret = SYSERR_OTHER;
        return;
    }

    if (*from_pid == currproc->pid) {
        currproc->ret = SYSPID_ME;
        return;
    }

    proc_ctrl_block_t *srcproc = pid_to_proc(*from_pid);
    if (*from_pid != 0 && srcproc == NULL) {
        currproc->ret = SYSPID_DNE;
        return;
    }

    if (*from_pid == 0) {
        currproc->ret = recv_any(currproc, buffer, len);
    } else {
        currproc->ret = recv(srcproc, currproc, buffer, len);
    }

    if (currproc->ret == SYSMSG_BLOCKED) {
        currproc->curr_state = PROC_STATE_BLOCKED;
        currproc = get_next_proc();
    }
}

/**
 * Handler for the syssleep syscall
 */
static void dispatch_syscall_sleep(void) {
    unsigned int milliseconds = currproc->args[0];
    if (milliseconds == 0) {
        return;
    }

    sleep(currproc, milliseconds);
    currproc = get_next_proc();
}

/**
 * Handler for timer events
 */
static void timer_handler(void) {
    currproc->cpu_time++;
    tick();
    add_pcb_to_queue(currproc, PROC_STATE_READY);
    currproc = get_next_proc();
    end_of_intr();
}

/**
 * Handler for sysgetcputime syscall
 * @return number of ticks consumed, or -1 if pid passed in does not exist
 */
static int dispatch_syscall_getcputime(void) {
    int pid = currproc->args[0];
    proc_ctrl_block_t* proc;

    if (pid == -1) {
        proc = currproc;
    } else if (pid == 0) {
        proc = get_idleproc();
    } else {
        proc = pid_to_proc(pid);
    }

    if (proc == NULL) {
        return -1;
    }

    return proc->cpu_time;
}
