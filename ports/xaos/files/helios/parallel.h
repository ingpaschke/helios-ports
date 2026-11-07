/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HELIOS_XAOS_PARALLEL_H
#define HELIOS_XAOS_PARALLEL_H
/* Called before XaoS parses its arguments. Nonzero means status+1. */
int hx_entry(int *, char **);
int hx_parallel_enabled(void);
int hx_processor_count(void);
int hx_profile_enabled(void);
int hx_preview_ready(void);
long hx_milliseconds(void);
/* Fill precisely the union of changed rows and columns in the current image.
 * An idle callback may request a preview interrupt. Pending jobs are drained;
 * completed rows/columns are cleared in the caller's dirty flags. Interrupted
 * passes describe unfinished work as row flags, preserving completed rows.
 * Coordinates and flags belong to the caller and are never retained. */
int hx_render(const number_t *,const number_t *,unsigned char *,
              unsigned char *,int (*)(void));
#endif
