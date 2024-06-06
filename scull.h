#ifndef _SCULL_H_
#define _SCULL_H_

#include <linux/ioctl.h>

#define SCULL_MAJOR 0
#define SCULL_NR_DEVS 4
#define SCULL_P_NR_DEVS 4
#define SCULL_QUANTUM 4000
#define SCULL_QSET 1000
#define SCULL_P_BUFFER 4000

extern int scull_major;
extern int scull_nr_devs;
extern int scull_quantum;
extern int scull_qset;

#endif
