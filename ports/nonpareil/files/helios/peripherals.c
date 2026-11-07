/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chip.h"
#include "sound.h"

/* The X11 front end has no printer, card-reader or audio interface. */
struct chip_t *gui_printer_install (struct sim_t *sim,
                            struct plugin_module_t *module,
                            chip_type_t type, int32_t index, int32_t flags)
{
  (void) sim;
  (void) module;
  (void) type;
  (void) index;
  (void) flags;
  fprintf (stderr, "Nonpareil: printer interface is unavailable\n");
  return NULL;
}

struct chip_t *gui_card_reader_install (struct sim_t *sim,
                                struct plugin_module_t *module,
                                chip_type_t type, int32_t index, int32_t flags)
{
  (void) sim;
  (void) module;
  (void) type;
  (void) index;
  (void) flags;
  fprintf (stderr, "Nonpareil: card reader is unavailable\n");
  return NULL;
}

sample_t squarewave_waveform_table[] = {0};
uint32_t squarewave_waveform_table_length = 1;

int synth_sound (float frequency, float amplitude, float duration,
                 sample_t *waveform, uint32_t length)
{
  (void) frequency;
  (void) amplitude;
  (void) duration;
  (void) waveform;
  (void) length;
  return -1;
}

bool stop_sound (int id)
{
  (void) id;
  return false;
}
