/* devtest.c : test code for devices

Called from outside:
  dev_run_all_tests() - runs all tests in this file

*/

#include <xerostest.h>
#include <xeroskernel.h>

void dev_run_all_tests(void) {
    char buf[20];
    
    DEBUG("buf addr: 0x%08x, sizeof(buf): %d\n", (unsigned long)buf, sizeof(buf));
    
    kprintf("sysopen: %d\n", sysopen(DEVICE_ID_KEYBOARD));
    kprintf("sysclose: %d\n", sysclose(0));
    kprintf("syswrite: %d\n", syswrite(0, &buf, sizeof(buf)));
    kprintf("sysread: %d\n", sysread(0, &buf, sizeof(buf)));
    kprintf("sysioctl: %d\n", sysioctl(0, 1));
    kprintf("sysioctl: %d\n", sysioctl(0, 1, 2));
    
    DEBUG("Done all device tests. Looping forever\n");
    while(1);
}