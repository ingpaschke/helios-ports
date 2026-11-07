/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "util.h"
#include "display.h"
#include "keyboard.h"
#include "chip.h"
#include "calcdef.h"
#include "proc.h"
#include "elapsed_time_us.h"
#include "keys.h"

#define KEY_SETTLE_US 30000

void np_keys_init (np_keys_t *keys)
{
  memset (keys, 0, sizeof (*keys));
  keys->physical_key = -1;
  keys->simulated_key = -1;
}

static void enqueue (np_keys_t *keys, int code, bool pressed)
{
  unsigned tail = (keys->head + keys->count) % NP_KEY_QUEUE_SIZE;

  /* The event loop leaves room for a release followed by another press. */
  if (keys->count == NP_KEY_QUEUE_SIZE)
    {
      fatal (2, "calculator input queue overflow\n");
    }
  keys->queue[tail].code = code;
  keys->queue[tail].pressed = pressed;
  keys->count++;
}

void np_keys_release (np_keys_t *keys)
{
  if (keys->physical_key >= 0)
    {
      enqueue (keys, keys->physical_key, false);
      keys->physical_key = -1;
      keys->xkey = 0;
    }
}

void np_keys_press (np_keys_t *keys, int code, unsigned xkey)
{
  if (code < 0 || code == keys->physical_key)
    {
      return;
    }
  np_keys_release (keys);
  enqueue (keys, code, true);
  keys->physical_key = code;
  keys->xkey = xkey;
}

void np_keys_tick (np_keys_t *keys, sim_t *sim)
{
  uint64_t now = get_elapsed_time_us ();
  np_key_event_t *event;

  if (!keys->count || now - keys->last_transition < KEY_SETTLE_US)
    {
      return;
    }
  /* X can deliver a press and release together. Give the simulated ROM
     time to scan and debounce each transition before taking the next one. */
  event = &keys->queue[keys->head];
  sim_key (sim, event->code, event->pressed);
  if (event->pressed)
    {
      keys->simulated_key = event->code;
    }
  else
    {
      keys->simulated_key = -1;
    }
  keys->head = (keys->head + 1) % NP_KEY_QUEUE_SIZE;
  keys->count--;
  keys->last_transition = now;
}

void np_keys_finish (np_keys_t *keys, sim_t *sim)
{
  if (keys->simulated_key >= 0)
    {
      sim_key (sim, keys->simulated_key, false);
    }
  np_keys_init (keys);
}
