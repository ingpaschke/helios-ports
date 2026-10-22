/* SPDX-License-Identifier: MIT */
#include <stdio.h>

void regerror(char *message)
{
    fprintf(stderr, "regexp: %s\n", message);
}
