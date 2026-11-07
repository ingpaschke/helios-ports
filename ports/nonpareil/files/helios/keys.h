/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NP_KEYS_H
#define NP_KEYS_H

#define NP_KEY_QUEUE_SIZE 128

typedef struct
{
  int code;
  bool pressed;
} np_key_event_t;

typedef struct
{
  np_key_event_t queue[NP_KEY_QUEUE_SIZE];
  unsigned head;
  unsigned count;
  int physical_key;
  unsigned xkey;
  int simulated_key;
  uint64_t last_transition;
} np_keys_t;

void np_keys_init (np_keys_t *keys);
void np_keys_press (np_keys_t *keys, int code, unsigned xkey);
void np_keys_release (np_keys_t *keys);
void np_keys_tick (np_keys_t *keys, sim_t *sim);
void np_keys_finish (np_keys_t *keys, sim_t *sim);

#endif
