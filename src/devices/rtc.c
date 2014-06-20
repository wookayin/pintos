#include "devices/rtc.h"
#include <stdio.h>
#include "threads/io.h"

/* This code is an interface to the MC146818A-compatible real
   time clock found on PC motherboards.  See [MC146818A] for
   hardware details. */

/* I/O register addresses. */
#define CMOS_REG_SET	0x70    /* Selects CMOS register exposed by REG_IO. */
#define CMOS_REG_IO	0x71    /* Contains the selected data byte. */

/* Indexes of CMOS registers with real-time clock functions.
   Note that all of these registers are in BCD format,
   so that 0x59 means 59, not 89. */
#define RTC_REG_SEC	0       /* Second: 0x00...0x59. */
#define RTC_REG_MIN	2       /* Minute: 0x00...0x59. */
#define RTC_REG_HOUR	4       /* Hour: 0x00...0x23. */
#define RTC_REG_MDAY	7	/* Day of the month: 0x01...0x31. */
#define RTC_REG_MON	8       /* Month: 0x01...0x12. */
#define RTC_REG_YEAR	9	/* Year: 0x00...0x99. */

/* Indexes of CMOS control registers. */
#define RTC_REG_A	0x0a    /* Register A: update-in-progress. */
#define RTC_REG_B	0x0b    /* Register B: 24/12 hour time, irq enables. */
#define RTC_REG_C	0x0c    /* Register C: pending interrupts. */
#define RTC_REG_D	0x0d    /* Register D: valid time? */

/* Register A. */
#define RTCSA_UIP	0x80	/* Set while time update in progress. */

/* Register B. */
#define	RTCSB_SET	0x80	/* Disables update to let time be set. */
#define RTCSB_DM	0x04	/* 0 = BCD time format, 1 = binary format. */
#define RTCSB_24HR	0x02    /* 0 = 12-hour format, 1 = 24-hour format. */

static int bcd_to_bin (uint8_t);
static uint8_t cmos_read (uint8_t index);

/* Returns number of seconds since Unix epoch of January 1,
   1970. */
time_t
rtc_get_time (void)
{
  static const int days_per_month[12] =
    {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
  int sec, min, hour, mday, mon, year;
  time_t time;
  int i;

  /* Get time components.

     We repeatedly read the time until it is stable from one read
     to another, in case we start our initial read in the middle
     of an update.  This strategy is not recommended by the
     MC146818A datasheet, but it is simpler than any of their
     suggestions and, furthermore, it is also used by Linux.

     The MC146818A can be configured for BCD or binary format,
     but for historical reasons everyone always uses BCD format
     except on obscure non-PC platforms, so we don't bother
     trying to detect the format in use. */
  do
    {
      sec = bcd_to_bin (cmos_read (RTC_REG_SEC));
      min = bcd_to_bin (cmos_read (RTC_REG_MIN));
      hour = bcd_to_bin (cmos_read (RTC_REG_HOUR));
      mday = bcd_to_bin (cmos_read (RTC_REG_MDAY));
      mon = bcd_to_bin (cmos_read (RTC_REG_MON));
      year = bcd_to_bin (cmos_read (RTC_REG_YEAR));
    }
  while (sec != bcd_to_bin (cmos_read (RTC_REG_SEC)));

  /* Translate years-since-1900 into years-since-1970.
     If it's before the epoch, assume that it has passed 2000.
     This will break at 2070, but that's long after our 31-bit
     time_t breaks in 2038. */
  if (year < 70)
    year += 100;
  year -= 70;

  /* Break down all components into seconds. */
  time = (year * 365 + (year - 1) / 4) * 24 * 60 * 60;
  for (i = 1; i <= mon; i++)
    time += days_per_month[i - 1] * 24 * 60 * 60;
  if (mon > 2 && year % 4 == 0)
    time += 24 * 60 * 60;
  time += (mday - 1) * 24 * 60 * 60;
  time += hour * 60 * 60;
  time += min * 60;
  time += sec;

  return time;
}

/* Returns the integer value of the given BCD byte. */
static int
bcd_to_bin (uint8_t x)
{
  return (x & 0x0f) + ((x >> 4) * 10);
}

/* Reads a byte from the CMOS register with the given INDEX and
   returns the byte read. */
static uint8_t
cmos_read (uint8_t index)
{
  outb (CMOS_REG_SET, index);
  return inb (CMOS_REG_IO);
}
