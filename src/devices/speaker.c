#include "devices/speaker.h"
#include "devices/pit.h"
#include "threads/io.h"
#include "threads/interrupt.h"
#include "devices/timer.h"

/* Speaker port enable I/O register. */
#define SPEAKER_PORT_GATE	0x61

/* Speaker port enable bits. */
#define SPEAKER_GATE_ENABLE	0x03

/* Sets the PC speaker to emit a tone at the given FREQUENCY, in
   Hz. */
void
speaker_on (int frequency)
{
  if (frequency >= 20 && frequency <= 20000)
    {
      /* Set the timer channel that's connected to the speaker to
         output a square wave at the given FREQUENCY, then
         connect the timer channel output to the speaker. */
      enum intr_level old_level = intr_disable ();
      pit_configure_channel (2, 3, frequency);
      outb (SPEAKER_PORT_GATE, inb (SPEAKER_PORT_GATE) | SPEAKER_GATE_ENABLE);
      intr_set_level (old_level);
    }
  else
    {
      /* FREQUENCY is outside the range of normal human hearing.
         Just turn off the speaker. */
      speaker_off ();
    }
}

/* Turn off the PC speaker, by disconnecting the timer channel's
   output from the speaker. */
void
speaker_off (void)
{
  enum intr_level old_level = intr_disable ();
  outb (SPEAKER_PORT_GATE, inb (SPEAKER_PORT_GATE) & ~SPEAKER_GATE_ENABLE);
  intr_set_level (old_level);
}

/* Briefly beep the PC speaker. */
void
speaker_beep (void)
{
  /* Only attempt to beep the speaker if interrupts are enabled,
     because we don't want to freeze the machine during the beep.
     We could add a hook to the timer interrupt to avoid that
     problem, but then we'd risk failing to ever stop the beep if
     Pintos crashes for some unrelated reason.  There's nothing
     more annoying than a machine whose beeping you can't stop
     without a power cycle.

     We can't just enable interrupts while we sleep.  For one
     thing, we get called (indirectly) from printf, which should
     always work, even during boot before we're ready to enable
     interrupts. */
  if (intr_get_level () == INTR_ON)
    {
      speaker_on (440);
      timer_msleep (250);
      speaker_off ();
    }
}
