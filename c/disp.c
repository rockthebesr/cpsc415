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
static void dispatch_syscall_wait(void);
static void dispatch_syscall_puts(void);
static void dispatch_syscall_send(void);
static void dispatch_syscall_recv(void);
static void dispatch_syscall_sleep(void);
static int dispatch_syscall_getcputimes(void);
static int dispatch_syscall_sighandler(void);
static void dispatch_syscall_sigreturn(void);
static int dispatch_syscall_open(void);
static int dispatch_syscall_close(void);
static int dispatch_syscall_write(void);
static int dispatch_syscall_read(void);
static int dispatch_syscall_ioctl(void);


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

        case SYSCALL_WAIT:
            dispatch_syscall_wait();
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

        case SYSCALL_CPUTIMES:
            currproc->ret = dispatch_syscall_getcputimes();
            break;

        case SYSCALL_SIGHANDLER:
            currproc->ret = dispatch_syscall_sighandler();
            break;

        case SYSCALL_SIGRETURN:
            dispatch_syscall_sigreturn();
            break;
        
        case SYSCALL_OPEN:
            dispatch_syscall_open();
            break;
            
        case SYSCALL_CLOSE:
            dispatch_syscall_close();
            break;
            
        case SYSCALL_WRITE:
            dispatch_syscall_write();
            break;
        
        case SYSCALL_READ:
            dispatch_syscall_read();
            break;
        
        case SYSCALL_IOCTL:
            dispatch_syscall_ioctl();
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
 * @return returns 0 on success, error code on failure
 */
static int dispatch_syscall_kill(void) {
    int pid = currproc->args[0];
    int signal = currproc->args[1];

    proc_ctrl_block_t* proc = pid_to_proc(pid);
    if (proc == NULL) {
        return SYSKILL_TARGET_DNE;
    }

    return set_proc_signal(proc, signal);
}

/**
 * Handler for the syswait syscall
 */
static void dispatch_syscall_wait(void) {
    int pid = currproc->args[0];

    proc_ctrl_block_t* proc_to_wait_on = pid_to_proc(pid);
    if (proc_to_wait_on == NULL) {
        currproc->ret = SYSPID_DNE;
        return;
    }

    currproc->curr_state = PROC_STATE_BLOCKED;
    add_proc_to_blocking_queue(currproc, proc_to_wait_on, WAITING);

    // setup all blocked procs' return values to assume
    // the target proc is eventually killed before resolution
    currproc->ret = 0;

    currproc = get_next_proc();
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

        // setup all blocked procs' return values to assume
        // the target proc is eventually killed before resolution
        currproc->ret = SYSPID_DNE;

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

        // setup all blocked procs' return values to assume
        // the target proc is eventually killed before resolution
        currproc->ret = SYSPID_DNE;

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
 * Handler for sysgetcputimes syscall
 * @return index of final slot filled within each of ps's arrays. -1 on error
 */
static int dispatch_syscall_getcputimes(void) {
    processStatuses *ps = (processStatuses*)currproc->args[0];
    if (verify_usrptr(ps, sizeof(processStatuses)) != OK) {
        return -1;
    }

    return get_all_proc_info(ps);
}

/**
 * Handler for syssighandler
 * @return returns 0 on success, or error code - see syssighandler for details
 */
static int dispatch_syscall_sighandler(void) {
    int signal = currproc->args[0];
    funcptr_args1 new_handler = (funcptr_args1)currproc->args[1];
    funcptr_args1 *old_handler = (funcptr_args1*)currproc->args[2];

    if (signal < 0 || signal >= SIGNAL_TABLE_SIZE) {
        return SYSHANDLER_INVALID_SIGNAL;
    }

    if (verify_usrptr(new_handler, sizeof(funcptr_args1)) != OK ||
        verify_usrptr(old_handler, sizeof(funcptr_args1*)) != OK) {
        return SYSHANDLER_INVALID_FUNCPTR;
    }

    *old_handler = currproc->signal_table[signal];
    currproc->signal_table[signal] = new_handler;
    return 0;
}

/**
 * Handler for syssigreturn
 */
static void dispatch_syscall_sigreturn(void) {
    void *old_sp = (void*)currproc->args[0];

    // it should only be faulty if the user modifies their stack
    // not able to return to original state at this point, so kill proc
    if (verify_usrptr(old_sp, sizeof(void*)) != OK) {
        cleanup_proc(currproc);
        currproc = get_next_proc();
        return;
    }

    // restore saved return value
    currproc->ret = *(int*)(((int)old_sp) - sizeof(int));

    currproc->esp = old_sp;
    currproc->signals_enabled = 1;
}

/**
 * Handler for sysopen
 */
static int dispatch_syscall_open(void) {
    // TODO: Implement me!
    return di_open();
}

/**
 * Handler for sysclose
 */
static int dispatch_syscall_close(void) {
    // TODO: Implement me!
    return di_close();
}

/**
 * Handler for syswrite
 */
static int dispatch_syscall_write(void) {
    // TODO: Implement me!
    return di_write();
}

/**
 * Handler for sysread
 */
static int dispatch_syscall_read(void) {
    // TODO: Implement me!
    return di_read();
}

/**
 * Handler for sysioctl
 */
static int dispatch_syscall_ioctl(void) {
    // TODO: Implement me!
    return di_ioctl();
}
