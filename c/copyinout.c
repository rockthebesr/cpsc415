/* copyinout.c : handles user pointers for system calls

Accessible through copyinout.h:
    verify_usrptr() - checks that user pointer falls within valid memory
    verify_usrstr() - checks that null-terminated user string is in valid memory

Further details can be found in the documentation above the function headers.
*/

#include <xeroskernel.h>
#include <i386.h>

static int falls_in_hole(long addr);
static int falls_in_kstack(long addr);

/**
 * Performs memory checks on a user pointer
 * @param usrptr: a user pointer passed in via syscall
 * @param len: length of data to check 
 * @return OK on success, EINVAL on bad pointer
 */
int verify_usrptr(void *usrptr, long len) {
    /* if len > KERNEL_STACK, we should rewrite our algorithm to handle the case
       where addr is below kstack, end addr is above kstack.
       Check kstack rather than hole, since kstack is much larger.       
    */
    ASSERT((len > 0) && (len < KERNEL_STACK));
    long addr = (long)usrptr;
    long end_addr = addr + len - 1;
    
    // must be within our memory
    if (addr <= 0 || end_addr > kmem_maxaddr()) {
        return EINVAL;
    }

    if (falls_in_hole(addr) || falls_in_hole(end_addr) ||
        falls_in_kstack(addr) || falls_in_kstack(end_addr)) {
        return EINVAL;
    }
    
    // unfortuantly, this is all the memory checking we can perform
    return OK;
}

/**
 * Checks if address is touching the HOLE
 * @param addr - address to check
 * @return true if the address falls within the HOLE
 */
static int falls_in_hole(long addr) {
    return (addr >= HOLESTART && addr < HOLEEND);
}

/**
 * Checks if address is within kernel stack
 * @param addr - address to check
 * @return true if the address falls within kstack
 */
static int falls_in_kstack(long addr) {
    return (addr >= (kmem_freemem() - KERNEL_STACK) && addr < kmem_freemem());
}


/**
 * Confirms user's null-terminated string falls entirely within valid memory
 * @param str: a user pointer passed by a syscall, to a string
 * @return OK on success, EINVAL otherwise
 */
int verify_usrstr(char *str) {
    int ret;

    do {
        ret = verify_usrptr(str, 1);
        if (ret != OK) {
            return ret;
        }
    } while(*str++ != '\0');

    return OK;
}
