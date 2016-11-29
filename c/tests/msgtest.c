/* msgtest.c : test code for msg

Called from outside:
  msg_run_all_tests() - runs all syscall tests

*/

#include <xerostest.h>
#include <xeroskernel.h>
#include <xeroslib.h>

#define MSGTEST_EXPECTED_NUM   0x1337F00D
#define STOP_SIGNAL 17

static void msgtest_basic_recver(void);
static void msgtest_basic_recver_any(void);
static void msgtest_basic_sender(void);
static void msgtest01_send_then_recv(void);
static void msgtest02_recv_then_send(void);
static void msgtest03_send_then_recv_any(void);
static void msgtest03_recv_any_then_send(void);
static void msgtest04_recv_any_queue(void);
static void msgtest05_send_bad_params(void);
static void msgtest06_recv_bad_params(void);
static void msgtest07_recv_any_queue_kill(void);
static void msgtest08_recv_out_of_order(void);
static void msgtest09_send_to_killed_proc(void);
static void msgtest10_recv_from_killed_proc(void);
static void msgtest11_sendbuf(void);

static int syskill_wrapper(int pid);
static void setup_stop_signal_handler(void);
static void msgtest_kill_itself(void);

static int g_recv_done_flag;
static int g_kill_this_pid;

void msg_run_all_tests(void) {
    msgtest01_send_then_recv();
    msgtest02_recv_then_send();
    msgtest03_send_then_recv_any();
    msgtest03_recv_any_then_send();
    msgtest04_recv_any_queue();
    msgtest05_send_bad_params();
    msgtest06_recv_bad_params();
    msgtest07_recv_any_queue_kill();
    msgtest08_recv_out_of_order();
    msgtest09_send_to_killed_proc();
    msgtest10_recv_from_killed_proc();
    msgtest11_sendbuf();
    
    kprintf("Done msg_run_all_tests, looping forever.\n");
    while(1);
}

/**
 * Sets up termination signal handler, calls signal on proc
 */
static int syskill_wrapper(int pid) {
    return syskill(pid, STOP_SIGNAL);
}

/**
 * Helper to setup sysstop as signal handler, to allow for proc killing
 */
static void setup_stop_signal_handler(void) {
    funcptr_args1 oldHandler;
    ASSERT_EQUAL(syssighandler(STOP_SIGNAL,
                               (funcptr_args1)&sysstop, &oldHandler), 0)
}

static void msgtest_basic_recver(void) {
    setup_stop_signal_handler();
    unsigned long num = 0xDEADBEEF;
    int pid = 33;
    int result = sysrecv(&pid, &num);
    g_recv_done_flag = 1;
    
    DEBUG("PID: %d\n", pid);
    DEBUG("num: %X\n", num);
    DEBUG("Result: %d\n", result);
    ASSERT_EQUAL(result, SYSPID_OK);
    ASSERT_EQUAL(num, MSGTEST_EXPECTED_NUM);
}

static void msgtest_basic_sender(void) {
    int pid = 33;
    int mypid = sysgetpid();
    setup_stop_signal_handler();

    int result = syssend(pid, (unsigned long)mypid);
    ASSERT_EQUAL(result, SYSPID_OK);
}

static void msgtest_killer(void) {
    setup_stop_signal_handler();
    sysyield();

    syskill_wrapper(g_kill_this_pid);
}

static void msgtest01_send_then_recv(void) {
    g_recv_done_flag = 0;
    
    int pid = syscreate(&msgtest_basic_recver, DEFAULT_STACK_SIZE);
    
    int result = syssend(pid, MSGTEST_EXPECTED_NUM);
    ASSERT_EQUAL(g_recv_done_flag, 1);
    DEBUG("PID: %d\n", pid);
    DEBUG("Result: %d\n", result);
    ASSERT_EQUAL(result, 0);
    
    for (int i = 0; i < 100; i++) {
        sysyield();
    }
}

static void msgtest02_recv_then_send(void) {
    g_recv_done_flag = 0;
    
    int pid = syscreate(&msgtest_basic_recver, DEFAULT_STACK_SIZE);
    sysyield();
    
    int result = syssend(pid, MSGTEST_EXPECTED_NUM);
    ASSERT_EQUAL(g_recv_done_flag, 0);
    DEBUG("PID: %d\n", pid);
    DEBUG("Result: %d\n", result);
    ASSERT_EQUAL(result, 0);
    
    for (int i = 0; i < 100; i++) {
        sysyield();
    }
}

static void msgtest03_send_then_recv_any(void) {
    g_recv_done_flag = 0;

    int pid = syscreate(&msgtest_basic_recver_any, DEFAULT_STACK_SIZE);
    DEBUG("Recv_any, receiver's PID: %d\n", pid);

    int result = syssend(pid, MSGTEST_EXPECTED_NUM);
    ASSERT_EQUAL(g_recv_done_flag, 1);
    DEBUG("Result: %d\n", result);
    ASSERT_EQUAL(result, 0);

    for (int i = 0; i < 100; i++) {
        sysyield();
    }
}

static void msgtest_basic_recver_any(void) {
    unsigned long num = 0xDEADBEEF;
    int pid = 0;
    int result = sysrecv(&pid, &num);
    g_recv_done_flag = 1;

    DEBUG("Recv_any, our PID: %d\n", sysgetpid());
    DEBUG("Recv_any, sender's PID: %d\n", pid);
    DEBUG("num: %X\n", num);
    DEBUG("Result: %d\n", result);
    ASSERT_EQUAL(result, 0);
    ASSERT_EQUAL(num, MSGTEST_EXPECTED_NUM);
}

static void msgtest03_recv_any_then_send(void) {
    g_recv_done_flag = 0;

    int pid = syscreate(&msgtest_basic_recver_any, DEFAULT_STACK_SIZE);
    sysyield();

    DEBUG("Recv_any, receiver's PID: %d\n", pid);
    int result = syssend(pid, MSGTEST_EXPECTED_NUM);
    ASSERT_EQUAL(g_recv_done_flag, 0);
    DEBUG("Result: %d\n", result);
    ASSERT_EQUAL(result, 0);

    for (int i = 0; i < 100; i++) {
        sysyield();
    }
}

/**
 * Queues up 10 sender processes. Then recvs from any.
 * Verifies that the senders are recv'd in order.
 */
static void msgtest04_recv_any_queue(void) {
    int i;
    int result;
    int pid;
    unsigned long num;
    int pids[10] = {0};
    
    for (i = 0; i < 10; i++) {
        pids[i] = syscreate(&msgtest_basic_sender, DEFAULT_STACK_SIZE);
        sysyield();
    }
    
    for (i = 0; i < 10; i++) {
        pid = 0;
        result = sysrecv(&pid, &num);
        ASSERT_EQUAL(result, SYSPID_OK);
        ASSERT_EQUAL(pid, pids[i]);
        ASSERT_EQUAL((int)num, pid);
    }
    
    for (int i = 0; i < 100; i++) {
        sysyield();
    }
}

static void msgtest05_send_bad_params(void) {
    unsigned long num = 0xDEADBEEF;
    int result;

    // basic send failures
    result = syssend(sysgetpid(), num);
    ASSERT_EQUAL(-2, result);
    DEBUG("send to self: %d\n", result);
    
    result = syssend(-1, num);
    ASSERT_EQUAL(-1, result);
    DEBUG("send to -1: %d\n", result);
    
    result = syssend(1994, num);
    ASSERT_EQUAL(-1, result);
    DEBUG("send to 1994: %d\n", result);
}

static void msgtest06_recv_bad_params(void) {
    int result;
    int pid = 9000;
    unsigned long num = 0xDEADBEEF;
    int our_pid = sysgetpid();
    int another_pid = syscreate(&msgtest_killer, DEFAULT_STACK_SIZE);
    sysyield();

    // basic recv failures
    result = sysrecv(&our_pid, &num);
    ASSERT_EQUAL(-2, result);
    DEBUG("recv from self: %d\n", result);
    
    result = sysrecv(&pid, &num);
    ASSERT_EQUAL(-1, result);
    DEBUG("recv from 9000: %d\n", result);
    
    result = sysrecv((void*)(-1), &num);
    ASSERT_EQUAL(-3, result);
    DEBUG("recv from (void*)-1: %d\n", result);
    
    result = sysrecv(&pid, (void*)(-1));
    ASSERT_EQUAL(-3, result);
    DEBUG("recv with bad buffer: %d\n", result);
    
    result = sysrecv(NULL, &num);
    ASSERT_EQUAL(-3, result);
    DEBUG("recv from NULL: %d\n", result);
    
    result = sysrecv(&another_pid, NULL);
    ASSERT_EQUAL(-3, result);
    DEBUG("recv from NULL buffer: %d\n", result);
    
    // cleanup
    syskill_wrapper(another_pid);
}

/**
 * Queues up 10 sender processes. Then kill half. Then receive from the rest.
 * Verifies that the senders are recv'd in order.
 */
static void msgtest07_recv_any_queue_kill(void) {
    int i;
    int result;
    int pid;
    unsigned long num;
    int pids[10] = {0};
    
    for (i = 0; i < 10; i++) {
        pids[i] = syscreate(&msgtest_basic_sender, DEFAULT_STACK_SIZE);
        sysyield();
    }
    
    // Kill half
    for (i = 0; i < 10; i += 2) {
        syskill_wrapper(pids[i]);
    }
    
    // Recv any from the remaining
    for (i = 1; i < 10; i += 2) {
        pid = 0;
        result = sysrecv(&pid, &num);
        ASSERT_EQUAL(result, SYSPID_OK);
        ASSERT_EQUAL(pid, pids[i]);
        ASSERT_EQUAL((int)num, pid);
        DEBUG("Received from PID %d\n", num);
    }
    
    for (int i = 0; i < 100; i++) {
        sysyield();
    }
}

/**
 * Queues up 10 sender processes. Then receive in staggered order
 */
static void msgtest08_recv_out_of_order(void) {
    int i;
    int result;
    int pid;
    unsigned long num;
    int pids[10] = {0};
    
    for (i = 0; i < 10; i++) {
        pids[i] = syscreate(&msgtest_basic_sender, DEFAULT_STACK_SIZE);
        sysyield();
    }
    
    // Specifically recv from half
    for (i = 9; i >= 0; i -= 2) {
        pid = pids[i];
        result = sysrecv(&pid, &num);
        ASSERT_EQUAL(result, SYSPID_OK);
        ASSERT_EQUAL(pid, pids[i]);
        ASSERT_EQUAL((int)num, pid);
        DEBUG("Received from PID %d\n", num);
    }
    
    // Recv any from the remaining
    for (i = 0; i < 10; i += 2) {
        pid = 0;
        result = sysrecv(&pid, &num);
        ASSERT_EQUAL(result, SYSPID_OK);
        ASSERT_EQUAL(pid, pids[i]);
        ASSERT_EQUAL((int)num, pid);
        DEBUG("Received from PID %d\n", num);
    }
    
    for (int i = 0; i < 100; i++) {
        sysyield();
    }
}

static void msgtest09_send_to_killed_proc(void) {
    int result;

    syscreate(&msgtest_killer, DEFAULT_STACK_SIZE);
    g_kill_this_pid = syscreate(&msgtest_kill_itself, DEFAULT_STACK_SIZE);
    sysyield();
    result = syssend(g_kill_this_pid, MSGTEST_EXPECTED_NUM);

    ASSERT_EQUAL(result, SYSPID_DNE);
    DEBUG("Result: %d\n", result);
}

static void msgtest10_recv_from_killed_proc(void) {
    int result;
    int pid;
    unsigned long num = 0xDEADBEEF;
    
    syscreate(&msgtest_killer, DEFAULT_STACK_SIZE);
    g_kill_this_pid = syscreate(&msgtest_kill_itself, DEFAULT_STACK_SIZE);
    sysyield();
    pid = g_kill_this_pid;
    result = sysrecv(&pid, &num);
    
    ASSERT_EQUAL(result, SYSPID_DNE);
    DEBUG("Result: %d\n", result);
}

static void msgtest_kill_itself(void) {
    setup_stop_signal_handler();
    sysyield();

    syskill_wrapper(sysgetpid());
    ASSERT(0);
}

static void msgtest_recvbuf_proc(void) {
    char buf[80] = {'\0'};
    int pid = 0;
    int result;
    
    result = sysrecvbuf(&pid, (void*)buf, sizeof(buf));
    ASSERT_EQUAL(result, SYSPID_OK);
    DEBUG("\nReceived (%d):\n%s\n", result, buf);
    
    sprintf(buf, "yeah im good it's jUsT A FLESHHHH AHHH~");
    result = syssendbuf(pid, (void*)buf, sizeof(buf));
    ASSERT_EQUAL(result, SYSPID_OK);
}

static void msgtest11_sendbuf(void) {
    int pid = syscreate(msgtest_recvbuf_proc, DEFAULT_STACK_SIZE);
    char buf[20] = {'\0'};
    unsigned long num;
    
    sprintf(buf, "Are you okay?");
    int result = syssendbuf(pid, (void*)buf, sizeof(buf) - 1);
    ASSERT_EQUAL(result, SYSPID_OK);
    
    result  = sysrecvbuf(&pid, (void*)buf, sizeof(buf) - 1);
    ASSERT_EQUAL(result, SYSPID_OK);
    DEBUG("\nReceived (%d):\n%s\n", result, buf);
    
    sysrecv(&pid, &num);
    DEBUG("Done.\n");
}
