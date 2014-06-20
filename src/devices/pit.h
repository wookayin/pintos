#ifndef DEVICES_PIT_H
#define DEVICES_PIT_H

#include <stdint.h>

void pit_configure_channel (int channel, int mode, int frequency);

#endif /* devices/pit.h */
