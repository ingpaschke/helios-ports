/* SPDX-License-Identifier: MIT */
/* Native Helios terminal adapter, kept separate from Xt's Object typedef. */
int helios_pty_open(int columns, int rows);
int helios_pty_spawn(char **command, unsigned long window, char *display, char *termname);
void helios_pty_resize(int fd, int columns, int rows);
int helios_pty_read(int fd, char *buffer, int size);
void helios_pty_close(void);
