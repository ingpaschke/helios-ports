#ifndef HELIOS_FVWM_COMMAND_H
#define HELIOS_FVWM_COMMAND_H
/* Split literal arguments in place. -1 means a shell must interpret it. */
int helios_command_split(char *, char **, unsigned);
#endif
