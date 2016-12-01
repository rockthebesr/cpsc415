/* devtest.c : test code for devices

Called from outside:
  dev_run_all_tests() - runs all tests in this file

*/

#include <xerostest.h>
#include <xeroskernel.h>

static void devtest_various(void);
static void devtest_open_close(void);

void dev_run_all_tests(void) {
    devtest_various();
    devtest_open_close();
    
    DEBUG("Done all device tests. Looping forever\n");
    while(1);
}

static void devtest_various(void) {
    char buf[20];
    DEBUG("buf addr: 0x%08x, sizeof(buf): %d\n", (unsigned long)buf, sizeof(buf));
    
    kprintf("syswrite: %d\n", syswrite(0, &buf, sizeof(buf)));
    kprintf("sysread: %d\n", sysread(0, &buf, sizeof(buf)));
    kprintf("sysioctl: %d\n", sysioctl(0, 1));
    kprintf("sysioctl: %d\n", sysioctl(0, 1, 2));
}

static void devtest_open_close(void) {
    int fd;
    int result;
    
    // Valid case: open and close the first FD
    fd = sysopen(DEVICE_ID_KEYBOARD);
    ASSERT_EQUAL(fd, 0);
    result = sysclose(fd);
    ASSERT_EQUAL(result, 0);
    
    // Error case: double close
    result = sysclose(fd);
    ASSERT_EQUAL(result, EBADF);
    
    // Error case: Random closes
    ASSERT_EQUAL(sysclose(-1), EBADF);
    ASSERT_EQUAL(sysclose(2), EBADF);
    ASSERT_EQUAL(sysclose(PCB_NUM_FDS), EBADF);
    ASSERT_EQUAL(sysclose(PCB_NUM_FDS + 1), EBADF);
}
