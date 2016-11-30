/* di_calls.c: Device calls

Called from outside:
    di_open() - opens device
    di_close() - close fd
    di_write() - write to device
    di_read() - read from device
    di_ioctl() - special control
    
Further details can be found in the documentation above the function headers.
 */

#include <xeroskernel.h>

/**
 * Handler for sysopen
 */
int di_open(void) {
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for sysclose
 */
int di_close(void) {
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for syswrite
 */
int di_write(void) {
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for sysread
 */
int di_read(void) {
    // TODO: Implement me!
    return -1;
}

/**
 * Handler for sysioctl
 */
int di_ioctl(void) {
    // TODO: Implement me!
    return -1;
}
