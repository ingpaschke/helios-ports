/* SPDX-License-Identifier: MIT */
/* Run with DISPLAY set, on an isolated display. */
#include <stdio.h>
#include <string.h>
#include <xview/xview.h>
#include <xview/canvas.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d\n",__LINE__); return 1; } } while (0)

int main(int argc, char **argv)
{
    Frame frame;
    Canvas canvas;
    Event first, second, saved;
    Event *result;
    xv_init(XV_INIT_ARGC_PTR_ARGV, &argc, argv, NULL);
    frame = xv_create(NULL,FRAME,XV_LABEL,"Event regression",NULL);
    CHECK(frame);
    canvas = xv_create(frame,CANVAS,XV_WIDTH,120,XV_HEIGHT,100,NULL);
    CHECK(canvas && xv_get(canvas,CANVAS_NTH_PAINT_WINDOW,0));
    memset(&first,0,sizeof(first)); memset(&second,0,sizeof(second));
    event_set_x(&first,17); event_set_y(&first,23);
    event_set_x(&second,31); event_set_y(&second,41);
    result = canvas_window_event(canvas,&first);
    CHECK(result==&first);
    saved = first;
    CHECK(canvas_window_event(canvas,&second)==&second);
    CHECK(memcmp(result,&saved,sizeof(saved))==0);
    CHECK(canvas_event(canvas,&first)==&first);
    saved = first;
    CHECK(canvas_event(canvas,&second)==&second);
    CHECK(memcmp(result,&saved,sizeof(saved))==0);
    xv_destroy(frame);
    puts("PASS caller-owned canvas events remain valid across calls");
    return 0;
}
