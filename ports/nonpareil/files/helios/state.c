/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "display.h"
#include "keyboard.h"
#include "chip.h"
#include "calcdef.h"
#include "proc.h"
#include "state.h"

static chip_t *chip_by_index (sim_t *sim, unsigned index)
{
  chip_t *chip = sim_get_next_chip (sim, NULL);

  while (chip && index)
    {
      chip = sim_get_next_chip (sim, chip);
      index--;
    }
  return chip;
}

static bool restore_record (sim_t *sim, const char *line, bool apply)
{
  char kind[16], data[32], register_name[64], trailing;
  unsigned chip_index, element, address;
  char *end;
  uint64_t value;

  if (sscanf (line, "%15s", kind) != 1 || kind[0] == '#')
    {
      return true;
    }
  if (!strcmp (kind, "register"))
    {
      chip_t *chip;
      const reg_info_t *info;
      int reg;

      if (sscanf (line, "register %u %63s %u %31s %c", &chip_index,
                   register_name, &element, data, &trailing) != 4)
        {
          return false;
        }
      chip = chip_by_index (sim, chip_index);
      if (!chip)
        {
          return false;
        }
      reg = sim_find_register (sim, chip, register_name);
      if (reg < 0)
        {
          return false;
        }
      info = sim_get_register_info (sim, chip, reg);
      if (!info || element >= (unsigned) info->array_element_count)
        {
          return false;
        }
      errno = 0;
      value = strtoull (data, &end, 16);
      if (errno || *end || end == data ||
          (info->element_bits < 64 && value >> info->element_bits))
        {
          return false;
        }
      if (apply)
        {
          return sim_write_register (sim, chip, reg, element, &value);
        }
      return true;
    }
  if (!strcmp (kind, "ram"))
    {
      uint64_t previous;

      if (sscanf (line, "ram %x %31s %c", &address, data, &trailing) != 2 ||
          address >= sim_get_max_ram_addr (sim) ||
          !sim_read_ram (sim, address, &previous))
        {
          return false;
        }
      errno = 0;
      value = strtoull (data, &end, 16);
      if (errno || *end || end == data || value >> 56)
        {
          return false;
        }
      if (apply)
        {
          return sim_write_ram (sim, address, &value);
        }
      return true;
    }
  return false;
}

static bool restore_file (sim_t *sim, FILE *input, const char *model, bool apply)
{
  char line[256], expected[64];

  snprintf (expected, sizeof (expected), "Nonpareil-state 1 %s\n", model);
  rewind (input);
  if (!fgets (line, sizeof (line), input) || strcmp (line, expected))
    {
      return false;
    }
  while (fgets (line, sizeof (line), input))
    {
      if (!strcmp (line, "end\n"))
        {
          return fgetc (input) == EOF && !ferror (input);
        }
      if (!strchr (line, '\n') || !restore_record (sim, line, apply))
        {
          return false;
        }
    }
  return false;
}

bool np_state_load (sim_t *sim, const char *filename, const char *model)
{
  FILE *input = fopen (filename, "r");
  bool valid;

  if (!input)
    {
      return false;
    }
  /* Validate all records before altering the running calculator. */
  valid = restore_file (sim, input, model, false);
  if (valid)
    {
      sim_event (sim, NULL, event_restore_starting, 0, 0, NULL);
      valid = restore_file (sim, input, model, true);
      sim_event (sim, NULL, event_restore_completed, 0, 0, NULL);
    }
  fclose (input);
  if (!valid)
    {
      fprintf (stderr, "Nonpareil: ignoring invalid state '%s'\n", filename);
      sim_reset (sim);
      sim_clear_memory (sim);
    }
  return valid;
}

bool np_state_save (sim_t *sim, const char *filename, const char *model)
{
  FILE *output;
  char temporary[512];
  const char *basename = strrchr (filename, '/');
  size_t directory_length = basename ? (size_t) (basename + 1 - filename) : 0;
  unsigned chip_index = 0;
  chip_t *chip = NULL;
  addr_t address;
  bool valid = true;

  if (directory_length + 13 > sizeof (temporary))
    {
      return false;
    }
  memcpy (temporary, filename, directory_length);
  snprintf (temporary + directory_length, sizeof (temporary) - directory_length,
             "%08x.tmp", (unsigned) getpid ());
  output = fopen (temporary, "w");
  if (!output)
    {
      return false;
    }
  sim_event (sim, NULL, event_save_starting, 0, 0, NULL);
  fprintf (output, "Nonpareil-state 1 %s\n", model);
  while ((chip = sim_get_next_chip (sim, chip)) != NULL)
    {
      int reg;

      fprintf (output, "# chip %u: %s\n", chip_index, sim_get_chip_info (sim, chip)->name);
      for (reg = 0; reg < sim_get_reg_count (sim, chip); reg++)
        {
          const reg_info_t *info = sim_get_register_info (sim, chip, reg);
          int element;

          for (element = 0; element < info->array_element_count; element++)
            {
              uint64_t value;
              char hex[32];

              if (!sim_read_register (sim, chip, reg, element, &value))
                {
                  valid = false;
                  continue;
                }
              snprintf (hex, sizeof (hex), "%016llx", (unsigned long long) value);
              fprintf (output, "register %u %s %u %s\n", chip_index,
                       info->name, element, hex);
            }
        }
      chip_index++;
    }
  for (address = 0; address < sim_get_max_ram_addr (sim); address++)
    {
      uint64_t value;
      char hex[32];

      if (sim_read_ram (sim, address, &value))
        {
          snprintf (hex, sizeof (hex), "%014llx", (unsigned long long) value);
          fprintf (output, "ram %x %s\n", address, hex);
        }
    }
  sim_event (sim, NULL, event_save_completed, 0, 0, NULL);
  fputs ("end\n", output);
  if (ferror (output))
    {
      valid = false;
    }
  if (fclose (output))
    {
      valid = false;
    }
  if (valid && rename (temporary, filename) == 0)
    {
      return true;
    }
  unlink (temporary);
  return false;
}
