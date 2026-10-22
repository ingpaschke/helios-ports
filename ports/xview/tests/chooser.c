/* SPDX-License-Identifier: MIT */
/* Run with DISPLAY set and an existing directory as the argument. */
#include <stdio.h>
#include <xview/xview.h>
#include <xview/file_chsr.h>
#include <xview/notify.h>

static Notify_value finished(Notify_client client, int timer)
{
    (void)client; (void)timer;
    notify_stop();
    return NOTIFY_DONE;
}

int main(int argc, char **argv)
{
    Frame frame, chooser;
    struct itimerval timer = {{0,0},{2,0}};
    xv_init(XV_INIT_ARGC_PTR_ARGV, &argc, argv, NULL);
    if (argc != 2) return 2;
    frame = xv_create(NULL, FRAME, XV_LABEL, "Chooser regression", NULL);
    chooser = xv_create(frame, FILE_CHOOSER,
                        FILE_CHOOSER_TYPE, FILE_CHOOSER_OPEN,
                        FILE_CHOOSER_DIRECTORY, argv[1], NULL);
    if (!chooser) return 3;
    puts("READY file chooser created"); fflush(stdout);
    xv_set(chooser, XV_SHOW, TRUE, NULL);
    notify_set_itimer_func((Notify_client)frame, finished, ITIMER_REAL,
                          &timer, NULL);
    xv_main_loop(frame);
    puts("PASS file chooser remained responsive");
    return 0;
}
