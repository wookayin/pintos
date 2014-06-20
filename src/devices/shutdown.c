#include "devices/shutdown.h"
#include <console.h>
#include <stdio.h>
#include "devices/kbd.h"
#include "devices/serial.h"
#include "devices/timer.h"
#include "threads/io.h"
#include "threads/thread.h"
#ifdef USERPROG
#include "userprog/exception.h"
#endif
#ifdef FILESYS
#include "devices/block.h"
#include "filesys/filesys.h"
#endif

/* Keyboard control register port. */
#define CONTROL_REG 0x64

/* How to shut down when shutdown() is called. */
static enum shutdown_type how = SHUTDOWN_NONE;

static void print_stats (void);

/* Shuts down the machine in the way configured by
   shutdown_configure().  If the shutdown type is SHUTDOWN_NONE
   (which is the default), returns without doing anything. */
void
shutdown (void)
{
  switch (how)
    {
    case SHUTDOWN_POWER_OFF:
      shutdown_power_off ();
      break;

    case SHUTDOWN_REBOOT:
      shutdown_reboot ();
      break;

    default:
      /* Nothing to do. */
      break;
    }
}

/* Sets TYPE as the way that machine will shut down when Pintos
   execution is complete. */
void
shutdown_configure (enum shutdown_type type)
{
  how = type;
}

/* Reboots the machine via the keyboard controller. */
void
shutdown_reboot (void)
{
  printf ("Rebooting...\n");

    /* See [kbd] for details on how to program the keyboard
     * controller. */
  for (;;)
    {
      int i;

      /* Poll keyboard controller's status byte until
       * 'input buffer empty' is reported. */
      for (i = 0; i < 0x10000; i++)
        {
          if ((inb (CONTROL_REG) & 0x02) == 0)
            break;
          timer_udelay (2);
        }

      timer_udelay (50);

      /* Pulse bit 0 of the output port P2 of the keyboard controller.
       * This will reset the CPU. */
      outb (CONTROL_REG, 0xfe);
      timer_udelay (50);
    }
}

/* Powers down the machine we're running on,
   as long as we're running on Bochs or QEMU. */
void
shutdown_power_off (void)
{
  const char s[] = "Shutdown";
  const char *p;

#ifdef FILESYS
  filesys_done ();
#endif

  print_stats ();

  printf ("Powering off...\n");
  serial_flush ();

  /* This is a special power-off sequence supported by Bochs and
     QEMU, but not by physical hardware. */
  for (p = s; *p != '\0'; p++)
    outb (0x8900, *p);

  /* For newer versions of qemu, you must run with -device
   * isa-debug-exit, which exits on any write to an IO port (by
   * default 0x501).  Qemu's exit code is double the value plus one,
   * so there is no way to exit cleanly.  We use 0x31 which should
   * result in a qemu exit code of 0x63.  */
  outb (0x501, 0x31);

  /* This will power off a VMware VM if "gui.exitOnCLIHLT = TRUE"
     is set in its configuration file.  (The "pintos" script does
     that automatically.)  */
  asm volatile ("cli; hlt" : : : "memory");

  /* None of those worked. */
  printf ("still running...\n");
  for (;;);
}

/* Print statistics about Pintos execution. */
static void
print_stats (void)
{
  timer_print_stats ();
  thread_print_stats ();
#ifdef FILESYS
  block_print_stats ();
#endif
  console_print_stats ();
  kbd_print_stats ();
#ifdef USERPROG
  exception_print_stats ();
#endif
}
