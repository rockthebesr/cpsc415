/* kbd.h : keyboard driver
   See kbd.c for further documentation
 */

#include <xeroskernel.h>

void kdb_devsw_create(devsw_t *entry, int echo_flag);
int kdb_init(void);
int kdb_read(void* buf, int buflen);
int kdb_write(void* buf, int buflen);
int kdb_ioctl(unsigned long command, ...);
int kdb_iint(void);
int kdb_oint(void);