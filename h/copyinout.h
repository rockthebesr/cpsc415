/* copyinout.h : handles user pointers for system calls
   See copyinout.c for further documentation
 */

#ifndef COPYINOUT_H
#define COPYINOUT_H

int verify_usrptr(void *usrptr, long len);
int verify_usrstr(char *usrstr);


#endif
