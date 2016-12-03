/* kbd.h : keyboard driver
   See kbd.c for further documentation
 */

#include <xeroskernel.h>

// Upper half
void kbd_devsw_create(devsw_t *entry, int echo_flag);
int kbd_init(void);
int kbd_open(proc_ctrl_block_t *proc, void *dvioblk);
int kbd_close(proc_ctrl_block_t *proc, void *dvioblk);
int kbd_read(proc_ctrl_block_t *proc, void *dvioblk, void* buf, int buflen);
int kbd_write(proc_ctrl_block_t *proc, void *dvioblk, void* buf, int buflen);
int kbd_ioctl(proc_ctrl_block_t *proc, void *dvioblk, unsigned long command, void *args);
int kbd_iint(void);
int kbd_oint(void);

// Lower half
void keyboard_isr(void);