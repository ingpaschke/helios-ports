/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise the real UI helper and zoom engine without an X display. */
#include <config.h>
#include <filter.h>
#include <ui_helper.h>
#include "parallel.h"
#include <stdio.h>

extern uih_context *uih;

static void (*normal_wait)(struct filter *);
static int cancelled;

static void cancel_render(struct filter *filter)
{
    if (filter->readyforinterrupt && filter->incalculation) {
        cancelled = 1;
        uih_interrupt(uih);
    }
    normal_wait(filter);
}

static int check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL %s\n", message);
        return 0;
    }
    return 1;
}

static void draw_animation_frame(void)
{
    uih->recalculatemode = UIH_ANIMATION;
    uih->maxtime = 1;
    uih_do_fractal(uih);
}

int main(int argc, char **argv)
{
    struct palette *palette;
    struct image *image;
    int action = hx_entry(&argc, argv);
    int passed = 1;

    if (action) {
        return action - 1;
    }
    if (!hx_parallel_enabled()) {
        fputs("Use -processors 2 or all to exercise the distributed path.\n", stderr);
        return 2;
    }
    palette = createpalette(0, 0, TRUECOLOR, 0, 0, NULL, NULL, NULL, NULL, NULL);
    if (!palette) {
        return 2;
    }
    image = create_image_mem(96, 64, 2, palette, 1.0, 1.0);
    if (!image) {
        destroypalette(palette);
        return 2;
    }
    uih = uih_mkcontext(0, image, NULL, NULL, NULL);
    if (!uih) {
        destroy_image(image);
        destroypalette(palette);
        return 2;
    }

    puts("PHASE initial image");
    fflush(stdout);
    uih_do_fractal(uih);

    puts("PHASE mouse held: adaptive preview");
    fflush(stdout);
    uih_recalculate(uih);
    uih->lastbuttons = BUTTON1;
    draw_animation_frame();
    passed &= check(uih->interruptiblemode, "mouse-held frame lost its preview budget");

    puts("PHASE mouse released: complete image");
    fflush(stdout);
    uih->lastbuttons = 0;
    uih->step = 0;
    draw_animation_frame();
    passed &= check(!uih->interruptiblemode, "stationary frame is still time-limited");
    passed &= check(!uih->uncomplette, "stationary frame did not finish");

    puts("PHASE zoom coasting");
    fflush(stdout);
    uih->step = 0.001;
    draw_animation_frame();
    passed &= check(uih->interruptiblemode, "coasting frame lost its preview budget");

    puts("PHASE continuous rotation");
    fflush(stdout);
    uih->step = 0;
    uih->rotatemode = ROTATE_CONTINUOUS;
    draw_animation_frame();
    passed &= check(uih->interruptiblemode, "rotation frame lost its preview budget");

    puts("PHASE cancel a stationary render");
    fflush(stdout);
    uih->rotatemode = ROTATE_NONE;
    uih_recalculate(uih);
    normal_wait = uih->zengine->wait_function;
    uih->zengine->wait_function = cancel_render;
    draw_animation_frame();
    passed &= check(cancelled && uih->uncomplette,
                    "explicit input could not interrupt a stationary render");
    uih->zengine->wait_function = normal_wait;
    draw_animation_frame();
    passed &= check(!uih->uncomplette, "render did not finish after cancellation");

    uih_freecontext(uih);
    uih = NULL;
    destroy_image(image);
    destroypalette(palette);
    puts(passed ? "PASS preview policy and zoom-engine integration" : "FAIL preview policy");
    return passed ? 0 : 1;
}
