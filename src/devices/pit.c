#include "devices/pit.h"
#include <debug.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/io.h"

/* Interface to 8254 Programmable Interrupt Timer (PIT).
   Refer to [8254] for details. */

/* 8254 registers. */
#define PIT_PORT_CONTROL          0x43                /* Control port. */
#define PIT_PORT_COUNTER(CHANNEL) (0x40 + (CHANNEL))  /* Counter port. */

/* PIT cycles per second. */
#define PIT_HZ 1193180

/* Configure the given CHANNEL in the PIT.  In a PC, the PIT's
   three output channels are hooked up like this:

     - Channel 0 is connected to interrupt line 0, so that it can
       be used as a periodic timer interrupt, as implemented in
       Pintos in devices/timer.c.

     - Channel 1 is used for dynamic RAM refresh (in older PCs).
       No good can come of messing with this.

     - Channel 2 is connected to the PC speaker, so that it can
       be used to play a tone, as implemented in Pintos in
       devices/speaker.c.

   MODE specifies the form of output:

     - Mode 2 is a periodic pulse: the channel's output is 1 for
       most of the period, but drops to 0 briefly toward the end
       of the period.  This is useful for hooking up to an
       interrupt controller to generate a periodic interrupt.

     - Mode 3 is a square wave: for the first half of the period
       it is 1, for the second half it is 0.  This is useful for
       generating a tone on a speaker.

     - Other modes are less useful.

   FREQUENCY is the number of periods per second, in Hz. */
void
pit_configure_channel (int channel, int mode, int frequency)
{
  uint16_t count;
  enum intr_level old_level;

  ASSERT (channel == 0 || channel == 2);
  ASSERT (mode == 2 || mode == 3);

  /* Convert FREQUENCY to a PIT counter value.  The PIT has a
     clock that runs at PIT_HZ cycles per second.  We must
     translate FREQUENCY into a number of these cycles. */
  if (frequency < 19)
    {
      /* Frequency is too low: the quotient would overflow the
         16-bit counter.  Force it to 0, which the PIT treats as
         65536, the highest possible count.  This yields a 18.2
         Hz timer, approximately. */
      count = 0;
    }
  else if (frequency > PIT_HZ)
    {
      /* Frequency is too high: the quotient would underflow to
         0, which the PIT would interpret as 65536.  A count of 1
         is illegal in mode 2, so we force it to 2, which yields
         a 596.590 kHz timer, approximately.  (This timer rate is
         probably too fast to be useful anyhow.) */
      count = 2;
    }
  else
    count = (PIT_HZ + frequency / 2) / frequency;

  /* Configure the PIT mode and load its counters. */
  old_level = intr_disable ();
  outb (PIT_PORT_CONTROL, (channel << 6) | 0x30 | (mode << 1));
  outb (PIT_PORT_COUNTER (channel), count);
  outb (PIT_PORT_COUNTER (channel), count >> 8);
  intr_set_level (old_level);
}
