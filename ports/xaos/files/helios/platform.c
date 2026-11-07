/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <syslib.h>
int hx_usleep(unsigned int microseconds)
{
    if (microseconds) Delay(microseconds);
    return 0;
}
