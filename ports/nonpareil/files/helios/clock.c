/* SPDX-License-Identifier: GPL-2.0-only */
#ifdef __HELIOS
#include <syslib.h>
#include <nonansi.h>
#endif

#include <stdbool.h>
#include <stdint.h>

#include "elapsed_time_us.h"

#ifdef __HELIOS
static uint32_t previous_tick;
static uint64_t elapsed;
static bool initialized;

bool elapsed_time_us_init (void)
{
  if (!initialized)
    {
      previous_tick = (uint32_t) _ldtimer (0);
      elapsed = 0;
      initialized = true;
    }
  return true;
}

uint64_t get_elapsed_time_us (void)
{
  uint32_t tick = (uint32_t) _ldtimer (0);

  /* The event loop samples the microsecond timer every 10 ms. Unsigned
     subtraction also handles its wrap every 71 minutes. */
  elapsed += (uint32_t) (tick - previous_tick);
  previous_tick = tick;
  return elapsed;
}

int np_usleep (unsigned int microseconds)
{
  if (microseconds)
    {
      Delay (microseconds);
    }
  return 0;
}
#else
#include <time.h>

static struct timespec start_time;

bool elapsed_time_us_init (void)
{
  return clock_gettime (CLOCK_MONOTONIC, &start_time) == 0;
}

uint64_t get_elapsed_time_us (void)
{
  struct timespec now;

  clock_gettime (CLOCK_MONOTONIC, &now);
  return (int64_t) (now.tv_sec - start_time.tv_sec) * 1000000 +
         (now.tv_nsec - start_time.tv_nsec) / 1000;
}
#endif
