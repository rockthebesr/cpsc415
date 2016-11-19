/* xerostest.h - contains each x_run_all_tests */

#ifndef XEROSTEST_H
#define XEROSTEST_H

#define BUSYWAIT() for (int i = 0; i < 2000000 * 5; i++);

// Hacky cleanup: yield a bunch of times so that all testfuncs have
#define MASS_SYSYIELD() for (int i = 0; i < 100; i++) sysyield();


void mem_run_all_tests(void);
void disp_run_all_tests(void);
void syscall_run_all_tests(void);
void copyinout_run_all_tests(void);
void msg_run_all_tests(void);
void timer_run_all_tests(void);

#endif
