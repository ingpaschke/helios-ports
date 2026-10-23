/* SPDX-License-Identifier: MIT */
/* Linked only into clock-oom; the production clock uses ordinary calloc. */
#include <stdio.h>
#include <stdlib.h>

void *clock_test_calloc(size_t count, size_t size)
{
    static int calls;
    const char *setting = getenv("XV_FAIL_ALLOC");
    if (setting && ++calls == atoi(setting)) {
        printf("FAILURE_INJECTED %d\n",calls);
        fflush(stdout);
        return NULL;
    }
    return calloc(count,size);
}
