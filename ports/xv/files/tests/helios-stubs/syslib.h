/* SPDX-License-Identifier: MIT
 * Native task identity declarations for host regression tests only.
 */
typedef long word;
#define TD_Port 4
word TaskData(word action, void *result);
word MachineName(char *name);
