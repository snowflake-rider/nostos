#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>

/* Starts the independent watchdog at an approximately four-second timeout. */
bool watchdog_start(void);
void watchdog_refresh(void);

#endif /* WATCHDOG_H */
