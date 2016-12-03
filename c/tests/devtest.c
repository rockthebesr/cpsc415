/* devtest.c : test code for devices

Called from outside:
  dev_run_all_tests() - runs all tests in this file

*/

#include <xerostest.h>
#include <xeroskernel.h>
#include <xeroslib.h>
#include <i386.h>

static void devtest_open_close(void);
static void devtest_write(void);
static void devtest_read(void);
static void devtest_read_ioctl(void);
static void devtest_read_err(void);
static void devtest_read_buffer(void);
static void devtest_read_multi_proc(void);
static void devtest_read_multi(void);
static void devtest_read_buffer_multi_proc(void);
static void devtest_read_buffer_multi(void);
static void devtest_multi_kill_cleanup_proc(void);
static void devtest_read_multi_kill_cleanup(void);
static void devtest_ioctl(void);

#define USER_KILL_SIGNAL 9

void dev_run_all_tests(void) {
    initPIT(1000 / TICK_LENGTH_IN_MS);
    
    devtest_open_close();
    devtest_write();
    devtest_read();
    devtest_read_err();
    devtest_read_ioctl();
    devtest_read_buffer();
    devtest_read_multi_kill_cleanup();
    devtest_read_multi();
    devtest_read_buffer_multi();
    devtest_ioctl();
    
    ASSERT_EQUAL(sysopen(DEVICE_ID_KEYBOARD), 0);
    kprintf("Done all device tests. Have fun with the keyboard!\n");
    while(1) {
        char buf[80];
        sysread(0, buf, sizeof(buf));
    }
}

static void devtest_open_close(void) {
    int fd;
    int fd2;
    
    // Valid case: open and close the first FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(sysclose(fd), 0);
    
    // Valid case: open and close the same device
    fd = sysopen(DEVICE_ID_KEYBOARD);
    fd2 = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(fd2, 1);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd2), 0);
    fd = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
    fd2 = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(fd2, 1);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd2), 0);
    
    // Error case: double close
    ASSERT_EQUAL(sysclose(fd), SYSERR);
    
    // Error case: Random closes
    ASSERT_EQUAL(sysclose(-1), SYSERR);
    ASSERT_EQUAL(sysclose(2), SYSERR);
    ASSERT_EQUAL(sysclose(PCB_NUM_FDS), SYSERR);
    ASSERT_EQUAL(sysclose(PCB_NUM_FDS + 1), SYSERR);
    
    // Error case: device does not exist
    fd = sysopen(-1);
    ASSERT_EQUAL(fd, SYSERR);
    fd = sysopen(40);
    ASSERT_EQUAL(fd, SYSERR);
    
    // only one of the keyboards can be open at a time
    fd = sysopen(DEVICE_ID_KEYBOARD);
    fd2 = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(fd2, SYSERR);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd2), SYSERR);
    
    fd = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
    fd2 = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(fd2, SYSERR);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd2), SYSERR);
    
    // opening too many fds will error
    ASSERT_EQUAL(sysopen(DEVICE_ID_KEYBOARD), 0);
    ASSERT_EQUAL(sysopen(DEVICE_ID_KEYBOARD), 1);
    ASSERT_EQUAL(sysopen(DEVICE_ID_KEYBOARD), 2);
    ASSERT_EQUAL(sysopen(DEVICE_ID_KEYBOARD), 3);
    ASSERT_EQUAL(sysopen(DEVICE_ID_KEYBOARD), -1);
    ASSERT_EQUAL(sysclose(0), 0);
    ASSERT_EQUAL(sysclose(1), 0);
    ASSERT_EQUAL(sysclose(2), 0);
    ASSERT_EQUAL(sysclose(3), 0);
}

static void devtest_write(void) {
    int fd;
    char buf[20];
    
    sprintf(buf, "Hello");
    
    // Valid case: open and write a FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(syswrite(fd, buf, strlen(buf)), -1);
    
    // Error case: write to a closed FD
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(syswrite(fd, buf, strlen(buf)), SYSERR);
    
    // Error case: write to arbitrary FDs
    ASSERT_EQUAL(syswrite(-1, buf, strlen(buf)), SYSERR);
    ASSERT_EQUAL(syswrite(2, buf, strlen(buf)), SYSERR);
    ASSERT_EQUAL(syswrite(PCB_NUM_FDS, buf, strlen(buf)), SYSERR);
    ASSERT_EQUAL(syswrite(PCB_NUM_FDS + 1, buf, strlen(buf)), SYSERR);
}

static void devtest_read(void) {
    int fd;
    char buf[20] = {'\0'};
    
    // Valid case: open and read a FD
    kprintf("Please type on the keyboard\n");
    fd = sysopen(DEVICE_ID_KEYBOARD);
    
    memset(buf, '\0', sizeof(buf));
    int bytes = sysread(fd, buf, 1);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 2);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 4);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 8);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 16);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    sysclose(fd);
}

static void devtest_read_ioctl(void) {
    int fd;
    int bytes;
    char buf[20] = {'\0'};
    
    // Valid case: open, read, and ioctl a FD
    kprintf("This should be silent (please type on keyboard)\n");
    fd = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 4);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    kprintf("Enabling echo... (please type on keyboard)\n");
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_ENABLE_ECHO), 0);
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 4);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    kprintf("Disabling echo... (please type on keyboard)\n");
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO), 0);
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 4);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    kprintf("Changing EOF to the character 'a'... (please type on keybaord)\n");
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_SET_EOF, 'a'), 0);
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 4);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    ASSERT_EQUAL(sysclose(fd), 0);
}

static void devtest_read_multi_proc(void) {
    int pid = sysgetpid();
    int fd;
    int bytes;
    char buf[4] = {'\0'};
    char printbuf[80];
    
    sprintf(printbuf, "pid %d starting read...\n", pid);
    sysputs(printbuf);
    fd = sysopen(DEVICE_ID_KEYBOARD);
    bytes = sysread(fd, buf, sizeof(buf) - 1);
    sprintf(printbuf, "[%d] (%d): %s\n", pid, bytes, buf);
    sysputs(printbuf);
    ASSERT_EQUAL(sysclose(fd), 0);
}

static void devtest_read_multi(void) {
    int pids[PCB_TABLE_SIZE - 1];
    int numProcs = 0;
    int i;
    
    for (i = 0; i < PCB_TABLE_SIZE - 1; i++) {
        pids[i] = syscreate(&devtest_read_multi_proc, DEFAULT_STACK_SIZE);
        numProcs += (pids[i] > 0 ? 1 : 0);
    }
    
    for (i = 0; i < PCB_TABLE_SIZE - 1; i++) {
        syswait(pids[i]);
    }
    
    DEBUG("Done! Test succeeded with %d processes\n", numProcs);
}

static void devtest_read_buffer(void) {
    int fd;
    int bytes;
    char buf[20] = {'\0'};
    
    // Valid case: open, sleep, then read. Buffer should flush
    fd = sysopen(DEVICE_ID_KEYBOARD);
    
    kprintf("Sleeping for 3 seconds (type to the keyboard now)...\n");
    syssleep(3000);
    kprintf("\nDone!\n");
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 2);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    kprintf("Sleeping for 3 seconds (type to the keyboard now)...\n");
    syssleep(3000);
    kprintf("\nDone!\n");
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 4);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    kprintf("Sleeping for 3 seconds (type to the keyboard now)...\n");
    syssleep(3000);
    kprintf("\nDone!\n");
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 8);
    kprintf("Returned (%d): %s\n", bytes, buf);
    
    ASSERT_EQUAL(sysclose(fd), 0);
}

static void devtest_read_buffer_multi_proc(void) {
    char buf = '\0';
    int fd;
    int pid = sysgetpid();
    int bytes;
    char printbuf[80];
    
    // Valid case: open, sleep, then read. Buffer should flush
    fd = sysopen(DEVICE_ID_KEYBOARD);
    
    syssleep(3000);
    bytes = sysread(fd, &buf, 1);
    sprintf(printbuf, "[%d] (%d): %c\n", pid, bytes, buf);
    sysputs(printbuf);
    
    ASSERT_EQUAL(sysclose(fd), 0);
}

static void devtest_read_buffer_multi(void) {
    int pids[PCB_TABLE_SIZE];
    int numProcs = 0;
    int i;
    
    DEBUG("Creating 5 processes...\nAll sleeping for 3 seconds (type to the keyboard now)...\n");
    for (i = 0; i < 5; i++) {
        pids[i] = syscreate(&devtest_read_buffer_multi_proc, DEFAULT_STACK_SIZE);
        numProcs += (pids[i] > 0 ? 1 : 0);
    }
    
    for (i = 0; i < 5 - 1; i++) {
        syswait(pids[i]);
    }
    
    DEBUG("Done! Test succeeded with %d processes\n", numProcs);
}

static void devtest_multi_kill_cleanup_proc(void) {
    funcptr_args1 oldHandler;
    char buf;
    int fd;
    int pid = sysgetpid();
    char printbuf[80];
    
    syssighandler(USER_KILL_SIGNAL,(funcptr_args1)&sysstop, &oldHandler);
    
    // Valid case: open, sleep, then read. Buffer should flush
    fd = sysopen(DEVICE_ID_KEYBOARD);
    
    syssleep(3000);
    ASSERT_EQUAL(sysread(fd, &buf, 1), 1);
    sprintf(printbuf, "[%d] (%d): %c\n", pid, 1, buf);
    sysputs(printbuf);
    
    ASSERT_EQUAL(sysclose(fd), 0);
}
static void devtest_read_multi_kill_cleanup(void) {
    int pids[5];
    int numProcs = 0;
    int i;
    
    DEBUG("Creating 5 processes... Don't type into keyboard...\n");
    for (i = 0; i < 5; i++) {
        pids[i] = syscreate(&devtest_multi_kill_cleanup_proc, DEFAULT_STACK_SIZE);
        numProcs += (pids[i] > 0 ? 1 : 0);
    }
    
    syssleep(1000);
    syskill(pids[2], USER_KILL_SIGNAL);
    syskill(pids[3], USER_KILL_SIGNAL);
    syskill(pids[5], USER_KILL_SIGNAL);
    DEBUG("Killed 3 processes. Please type now\n");
    
    for (i = 0; i < 5; i++) {
        syswait(pids[i]);
    }
    DEBUG("Done\n");
}
static void devtest_read_err(void) {
    int fd;
    char buf[4];
    
    // Error case: read to a closed FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysread(fd, buf, sizeof(buf)), SYSERR);
    
    // Error case: read to arbitrary FDs
    ASSERT_EQUAL(sysread(-1, buf, sizeof(buf)), SYSERR);
    ASSERT_EQUAL(sysread(2, buf, sizeof(buf)), SYSERR);
    ASSERT_EQUAL(sysread(PCB_NUM_FDS, buf, sizeof(buf)), SYSERR);
    ASSERT_EQUAL(sysread(PCB_NUM_FDS + 1, buf, sizeof(buf)), SYSERR);
}

static void devtest_ioctl(void) {
    int fd;
    
    // Valid case: open and ioctl a FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_SET_EOF, 'a'), 0);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_EOF), (int)'a');
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_ENABLE_ECHO), 0);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_ECHO), 1);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO), 0);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_ECHO), 0);
    
    // Invalid case: Random ioctl command codes
    ASSERT_EQUAL(sysioctl(fd, 1), SYSERR);
    ASSERT_EQUAL(sysioctl(fd, -1), SYSERR);
    ASSERT_EQUAL(sysioctl(fd, 0), SYSERR);
    
    // Invalid case: ioctl without the parameter... just make sure we don't crash
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_SET_EOF), 0);
    
    // Invalid case: ioctl on closed FD
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_SET_EOF, 'a'), SYSERR);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_EOF), SYSERR);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_ENABLE_ECHO), SYSERR);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_ECHO), SYSERR);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO), SYSERR);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_ECHO), SYSERR);
}
