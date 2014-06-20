#ifndef DEVICES_SHUTDOWN_H
#define DEVICES_SHUTDOWN_H

#include <debug.h>

/* How to shut down when Pintos has nothing left to do. */
enum shutdown_type
  {
    SHUTDOWN_NONE,              /* Loop forever. */
    SHUTDOWN_POWER_OFF,         /* Power off the machine (if possible). */
    SHUTDOWN_REBOOT,            /* Reboot the machine (if possible). */
  };

void shutdown (void);
void shutdown_configure (enum shutdown_type);
void shutdown_reboot (void) NO_RETURN;
void shutdown_power_off (void) NO_RETURN;

#endif /* devices/shutdown.h */
