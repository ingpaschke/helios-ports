/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NP_SKIN_H
#define NP_SKIN_H

#include <X11/Xlib.h>

#define NP_MAX_BUTTONS 40
#define NP_MAX_BINDINGS 80
#define NP_DIGITS 11
#define NP_SEGMENTS 9

typedef struct
{
  int code;
  int x, y, width, height;
} np_button_t;

typedef struct
{
  KeySym symbol;
  int code;
} np_binding_t;

typedef struct
{
  Pixmap mask;
  int x, y, width, height;
} np_annunciator_t;

typedef struct
{
  char title[128];
  int width, height;
  int display_x, display_y, display_width, display_height;
  int digit_width, digit_height, digit_x, digit_y;
  unsigned long foreground, background;
  Pixmap face;
  Pixmap segments[NP_SEGMENTS];
  np_annunciator_t annunciators[NP_DIGITS];
  np_button_t buttons[NP_MAX_BUTTONS];
  int button_count;
  np_binding_t bindings[NP_MAX_BINDINGS];
  int binding_count;
} np_skin_t;

void np_skin_load (Display *display, int screen, const char *directory,
                   np_skin_t *skin);
void np_skin_free (Display *display, np_skin_t *skin);
int np_button_at (const np_skin_t *skin, int x, int y);
int np_key_binding (const np_skin_t *skin, KeySym symbol);

#endif
