/* devtest.c : test code for devices

Called from outside:
  dev_run_all_tests() - runs all tests in this file

*/

#include <xerostest.h>
#include <xeroskernel.h>
#include <xeroslib.h>

static void devtest_open_close(void);
static void devtest_write(void);
static void devtest_read(void);
static void devtest_read_err(void);
static void devtest_ioctl(void);

void dev_run_all_tests(void) {
    devtest_open_close();
    devtest_write();
    devtest_read();
    devtest_read_err();
    devtest_ioctl();
    
    ASSERT_EQUAL(sysopen(DEVICE_ID_KEYBOARD), 0);
    DEBUG("Done all device tests. Have fun with the keyboard!\n");
    while(1);
}

static void devtest_open_close(void) {
    int fd;
    int fd2;
    
    // Valid case: open and close the first FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(sysclose(fd), 0);
    
    // Error case: double close
    ASSERT_EQUAL(sysclose(fd), EBADF);
    
    // Error case: Random closes
    ASSERT_EQUAL(sysclose(-1), EBADF);
    ASSERT_EQUAL(sysclose(2), EBADF);
    ASSERT_EQUAL(sysclose(PCB_NUM_FDS), EBADF);
    ASSERT_EQUAL(sysclose(PCB_NUM_FDS + 1), EBADF);
    
    // Error case: open twice
    fd = sysopen(DEVICE_ID_KEYBOARD);
    fd2 = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(fd2, EBUSY);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd2), EBADF);
    
    fd = sysopen(DEVICE_ID_KEYBOARD);
    fd2 = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(fd2, EBUSY);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd2), EBADF);
    
    fd = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
    fd2 = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(fd2, EBUSY);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysclose(fd2), EBADF);
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
    ASSERT_EQUAL(syswrite(fd, buf, strlen(buf)), EBADF);
    
    // Error case: write to arbitrary FDs
    ASSERT_EQUAL(syswrite(-1, buf, strlen(buf)), EBADF);
    ASSERT_EQUAL(syswrite(2, buf, strlen(buf)), EBADF);
    ASSERT_EQUAL(syswrite(PCB_NUM_FDS, buf, strlen(buf)), EBADF);
    ASSERT_EQUAL(syswrite(PCB_NUM_FDS + 1, buf, strlen(buf)), EBADF);
}

static void devtest_read(void) {
    int fd;
    int bytes;
    char buf[20] = {'\0'};
    
    // Valid case: open and read a FD
    DEBUG("Please type on the keyboard\n");
    fd = sysopen(DEVICE_ID_KEYBOARD);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 1);
    DEBUG("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 2);
    DEBUG("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 4);
    DEBUG("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 8);
    DEBUG("Returned (%d): %s\n", bytes, buf);
    
    memset(buf, '\0', sizeof(buf));
    bytes = sysread(fd, buf, 16);
    DEBUG("Returned (%d): %s\n", bytes, buf);
    
    sysclose(fd);
}

static void devtest_read_err(void) {
    int fd;
    char buf[4];
    
    // Error case: read to a closed FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysread(fd, buf, sizeof(buf)), EBADF);
    
    // Error case: read to arbitrary FDs
    ASSERT_EQUAL(sysread(-1, buf, sizeof(buf)), EBADF);
    ASSERT_EQUAL(sysread(2, buf, sizeof(buf)), EBADF);
    ASSERT_EQUAL(sysread(PCB_NUM_FDS, buf, sizeof(buf)), EBADF);
    ASSERT_EQUAL(sysread(PCB_NUM_FDS + 1, buf, sizeof(buf)), EBADF);
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
    ASSERT_EQUAL(sysioctl(fd, 1), ENOIOCTLCMD);
    ASSERT_EQUAL(sysioctl(fd, -1), ENOIOCTLCMD);
    ASSERT_EQUAL(sysioctl(fd, 0), ENOIOCTLCMD);
    
    // Invalid case: ioctl without the parameter... just make sure we don't crash
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_SET_EOF), 0);
    
    // Invalid case: ioctl on closed FD
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_SET_EOF, 'a'), EBADF);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_EOF), EBADF);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_ENABLE_ECHO), EBADF);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_ECHO), EBADF);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_DISABLE_ECHO), EBADF);
    ASSERT_EQUAL(sysioctl(fd, KEYBOARD_IOCTL_GET_ECHO), EBADF);
}
