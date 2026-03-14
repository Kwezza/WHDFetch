#ifndef TIMER_SHARED_H
#define TIMER_SHARED_H

#include <exec/types.h>
#include <devices/timer.h>

/* Global timer base declaration that will be used by all files */
extern struct Device *TimerBase;

/* Common timer initialization and cleanup functions */
BOOL InitTimer(void);
void CleanupTimer(void);

#endif /* TIMER_SHARED_H */