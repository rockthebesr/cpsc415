Our version of sysgetcputimes() returns -1 on any invalid pointer.
This was done for consistency, and to avoid breaking our encapsulation of
user pointer checks. This was endorsed by Donald Acton on Piazza:

https://piazza.com/class/isp6lqkqfq32kx?cid=272

---

Our version of sysopen allows the keyboard device to be open multiple times
as long as the same keyboard type is specified. For example:
```c
// This is allowed by our kernel
int fd1 = sysopen(DEVICE_ID_KEYBOARD);
int fd2 = sysopen(DEVICE_ID_KEYBOARD);
int fd3 = sysopen(DEVICE_ID_KEYBOARD);

// This is not allowed by our kernel
int fd4 = sysopen(DEVICE_ID_KEYBOARD);
int fd5 = sysopen(DEVICE_ID_KEYBOARD_NO_ECHO);
```
In the event that multiple processes listen on the keyboard at the same time,
they will all receive the same keyboard characters.