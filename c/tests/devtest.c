/* devtest.c : test code for devices

Called from outside:
  dev_run_all_tests() - runs all tests in this file

*/

#include <xerostest.h>
#include <xeroskernel.h>

void dev_run_all_tests(void) {
    DEBUG("sysopen: %d\n", sysopen(DEVICE_ID_KEYBOARD));
    DEBUG("sysclose: %d\n", sysclose(0));
    DEBUG("syswrite: %d\n", syswrite(0, NULL, 0));
    DEBUG("sysread: %d\n", sysread(0, NULL, 0));
    DEBUG("sysioctl: %d\n", sysioctl(0, 1));
    DEBUG("sysioctl: %d\n", sysioctl(0, 1, 2));
    
    DEBUG("Done all device tests. Looping forever\n");
    while(1);
}