#ifndef DEBUG_H
#define DEBUG_H

#include "lib.h"

#ifdef DEBUG
#define DEBUG_D(var) \
    do { \
        printk("[DEBUG] " #var ": %d\n", var); \
    } while (0)
#define DEBUG_X(var) \
    do { \
        printk("[DEBUG] " #var ": %x\n", var); \
    } while (0)
#define DEBUG_F(var) \
    do { \
        printk("[DEBUG] " #var ": %f\n", var); \
    } while (0)

#define DEBUG_DM(var, msg) \
    do { \
        printk("[DEBUG] " msg ": %d\n", var); \
    } while (0)
#define DEBUG_XM(var, msg) \
    do { \
        printk("[DEBUG] " msg ": %x\n", var); \
    } while (0)
#define DEBUG_FM(var, msg) \
    do { \
        printk("[DEBUG] " msg ": %f\n", var); \
    } while (0)
#else
#define DEBUG_D(var)
#define DEBUG_X(var)
#define DEBUG_F(var)
#define DEBUG_DM(var, msg)
#define DEBUG_XM(var, msg)
#define DEBUG_FM(var, msg)
#endif

#endif
