/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NP_STATE_H
#define NP_STATE_H

bool np_state_load (sim_t *sim, const char *filename, const char *model);
bool np_state_save (sim_t *sim, const char *filename, const char *model);

#endif
