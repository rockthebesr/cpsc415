/* kbd.c: Keyboard device specific code
*/

#include <xeroslib.h>
#include <kbd.h>

// TODO: Default EOF should be ctrl-D
#define KBD_DEFAULT_EOF 0

typedef struct kdb_dvioblk {
    char eof;
    int echo_flag; // 1 for on, 0 for off
} kdb_dvioblk_t;

/**
 * Fills in a device table entry with keyboard-device specific values
 * @param entry - device table entry to be modified
 */
void kdb_devsw_create(devsw_t *entry, int echo_flag) {
    ASSERT(entry != NULL);
    
    sprintf(entry->dvname, "keyboard");
    entry->dvinit = &kdb_init;
    entry->dvread = &kdb_read;
    entry->dvwrite = &kdb_write;
    entry->dvioctl = &kdb_ioctl;
    entry->dviint = &kdb_iint;
    entry->dvoint = &kdb_oint;
    entry->dvioblk = (kdb_dvioblk_t*)kmalloc(sizeof(kdb_dvioblk_t));
    ASSERT(entry->dvioblk != NULL);
    ((kdb_dvioblk_t*)entry->dvioblk)->eof = (char)KBD_DEFAULT_EOF;
    ((kdb_dvioblk_t*)entry->dvioblk)->echo_flag = echo_flag;
}

/**
 * Implementations of devsw abstract functions
 */

int kdb_init(void) {
    DEBUG("\n");
    return -1;
}

int kdb_read(void* buf, int buflen) {
    DEBUG("\n");
    return -1;
}

int kdb_write(void* buf, int buflen) {
    DEBUG("\n");
    return -1;
}

int kdb_ioctl(unsigned long command, ...) {
    DEBUG("\n");
    return -1;
}

// input available interrupt
int kdb_iint(void) {
    DEBUG("\n");
    return -1;
}

// output available interrupt
int kdb_oint(void) {
    DEBUG("\n");
    return -1;
}
