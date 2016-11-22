Our version of sysgetcputimes() returns -1 on any invalid pointer.
This was done for consistency, and to avoid breaking our encapsulation of
user pointer checks. This was endorsed by Donald Acton on Piazza:

https://piazza.com/class/isp6lqkqfq32kx?cid=272
