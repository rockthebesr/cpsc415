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
static void devtest_ioctl(void);

void dev_run_all_tests(void) {
    devtest_open_close();
    devtest_write();
    devtest_read();
    devtest_ioctl();
    
    DEBUG("Done all device tests. Looping forever\n");
    while(1);
}

static void devtest_open_close(void) {
    int fd;
    
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
}

static void devtest_write(void) {
    int fd;
    char buf[20];
    
    DEBUG("buf addr: 0x%08x, sizeof(buf): %d\n", (unsigned long)buf, sizeof(buf));
    sprintf(buf, "Hello");
    
    // Valid case: open and write a FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(syswrite(fd, buf, strlen(buf)), 0);
    
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
    char buf[20];
    
    DEBUG("buf addr: 0x%08x, sizeof(buf): %d\n", (unsigned long)buf, sizeof(buf));
    sprintf(buf, "Hello");
    
    // Valid case: open and read a FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    ASSERT_EQUAL(sysread(fd, buf, strlen(buf)), 0);
    
    // Error case: read to a closed FD
    ASSERT_EQUAL(sysclose(fd), 0);
    ASSERT_EQUAL(sysread(fd, buf, strlen(buf)), EBADF);
    
    // Error case: read to arbitrary FDs
    ASSERT_EQUAL(sysread(-1, buf, strlen(buf)), EBADF);
    ASSERT_EQUAL(sysread(2, buf, strlen(buf)), EBADF);
    ASSERT_EQUAL(sysread(PCB_NUM_FDS, buf, strlen(buf)), EBADF);
    ASSERT_EQUAL(sysread(PCB_NUM_FDS + 1, buf, strlen(buf)), EBADF);
}

static void devtest_ioctl(void) {
    int fd;
    
    // Valid case: open and ioctl a FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(sysioctl(0, 1, -1), 0);
    ASSERT_EQUAL(sysioctl(0, 1, 2, -1), 0);
    ASSERT_EQUAL(sysioctl(0, 1, 2, 3, -1), 0);
    ASSERT_EQUAL(sysclose(fd), 0);
}
