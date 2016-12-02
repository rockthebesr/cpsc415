/* user.c : User processes
 */

#include <xeroskernel.h>
#include <xeroslib.h>

static int g_root_proc_pid;

/**
 * Root process
 */
void root(void) {
    kprintf("Hello world!\n");
    syscreate(&producer, DEFAULT_STACK_SIZE);
    syscreate(&consumer, DEFAULT_STACK_SIZE);
    for (;;) sysyield();
}

/**
 * Producer
 */
void producer(void) {
    int i;
    for (i = 0; i < 12; i++) {
        kprintf("Happy 101st\n");
        sysyield();
    }
    sysstop();    
}

/**
 * Consumer
 */
void consumer(void) {
    int i;
    for (i = 0; i < 15; i++) {
        kprintf("Birthday UBC\n");
        sysyield();
    }
    sysstop();  
}

/**
 * Child
 * As required by the asst2 spec:
 * * print a message indicating it is alive
 * * sleep for 5 seconds
 * * recv() from root process, this is the # of ms to sleep
 * * print a message saying the message was received, specify the sleep time
 * * print message saying sleeping has stopped and is going to exit
 * * runs off the end of its code
 */
void child(void) {
    char buf[80];
    int result;
    unsigned long sleep_time;
    int root = g_root_proc_pid;
    int pid = sysgetpid();
    
    sprintf(buf, "[%d]: Hello! I am alive!\n", pid);
    sysputs(buf);
    syssleep(5000);
    
    result = sysrecv(&root, &sleep_time);
    if (result != SYSPID_OK) {
        sprintf(buf, "[%d]: Error %d: Could not receive sleep_time from root\n",
            pid, result);
        sysputs(buf);
        goto done;
    }
    sprintf(buf, "[%d]: Received sleep_time from root: %lu\n",
        pid, sleep_time);
    sysputs(buf);
    syssleep(sleep_time);
    
done:
    sprintf(buf, "[%d]: Terminating, goodbye\n", pid);
    sysputs(buf);
}

/**
 * Parent (AKA root)
 * As required by the asst2 spec:
 * * prints a message saying it is alive
 * * create 4 children, printing their pids
 * * sleeps for 4 seconds
 * * tell proc3 to sleep for 10 seconds
 * * tell proc2 to sleep for 7 seconds
 * * tell proc1 to sleep for 20 seconds
 * * tell proc4 to sleep for 27 seconds
 * * try to recv from proc4, print resulting error code
 * * try to send to proc3, print resulting error code
 * * call sysstop() explicitly
 */
void parent(void) {
    char buf[80];
    int result;
    int children[4];
    int i;
    unsigned long msg;
    
    g_root_proc_pid = sysgetpid();
    
    sprintf(buf, "[root]: Hello! I am root! (pid: %d)\n", g_root_proc_pid);
    sysputs(buf);
    for (i = 0; i < 4; i++) {
        children[i] = syscreate(&child, DEFAULT_STACK_SIZE);
        if (children[i] <= 0) {
            sprintf(buf, "[root]: Error creating child %d\n", i);
            sysputs(buf);
        }
        sprintf(buf, "[root]: Created child with PID %d\n", children[i]);
        sysputs(buf);
    }
    syssleep(4000);
    
    syssend(children[2], 10000);
    syssend(children[1], 7000);
    syssend(children[0], 20000);
    syssend(children[3], 27000);
    
    result = sysrecv(&children[3], &msg);
    sprintf(buf, "[root]: recv from %d resulted in return code: %d\n",
        children[3], result);
    sysputs(buf);
    
    result = syssend(children[2], (unsigned long)0xcafecafe);
    sprintf(buf, "[root]: send to %d resulted in return code: %d\n",
        children[2], result);
    sysputs(buf);
    
    sysputs("Done.\n");
    sysstop();
}

void shell(void) {
    sysputs("shell\n");
}

/**
 * Authenticates the user, starts the shell process
 */
void login_proc(void) {
    char* valid_user = "cpsc415";
    char* valid_pass = "EveryoneGetsAnA";

    while(1) {
        char user_buf[8];
        char pass_buf[16];
        memset(user_buf, '\0', sizeof(user_buf));
        memset(pass_buf, '\0', sizeof(pass_buf));

        sysputs("Welcome to Xeros - an experimental OS\n");
        int fd = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);

        sysioctl(fd, KEYBOARD_IOCTL_ENABLE_ECHO);
        sysputs("Username: ");
        sysread(fd, user_buf, 8);

        //        sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO);
        sysputs("\nPassword: \n");
        sysread(fd, pass_buf, 19);
        sysclose(fd);

        kprintf("!%s!\n", user_buf);
        kprintf("!%s!\n", pass_buf);

        if (strcmp(user_buf, valid_user) == 0 &&
            strcmp(pass_buf, valid_pass) == 0) {
            int shell_pid = syscreate(&shell, DEFAULT_STACK_SIZE);
            syswait(shell_pid);
        }
    }
}
