/* SPDX-License-Identifier: GPL-2.0-only */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "util.h"
#include "display.h"
#include "keyboard.h"
#include "chip.h"
#include "calcdef.h"
#include "proc.h"
#include "skin.h"
#include "state.h"
#include "keys.h"

#ifndef NP_DATA_DIR
#define NP_DATA_DIR "/helios/local/share/nonpar"
#endif

char *nonpareil_release = "Nonpareil 0.79 for Helios";
static volatile sig_atomic_t interrupted;

typedef struct
{
  Display *display;
  Window window;
  GC gc;
  np_skin_t skin;
  sim_t *sim;
  segment_bitmap_t segments[NP_DIGITS];
  bool display_changed;
  np_keys_t keys;
} calculator_t;

/* The simulator core exposes this scheduler entry point to its GTK thread.
   The native front end calls it from the X11 event loop instead. */
extern void sim_run (sim_t *sim);

void usage (FILE *output)
{
  fprintf (output, "%s\n", nonpareil_release);
  fprintf (output, "Usage: nonpar [10c|11c|12c|15c|16c] [-display DISPLAY]\n");
  fprintf (output, "       [-data DIRECTORY] [-state FILE|-nostate] [-test]\n");
  fprintf (output, "Mouse: press calculator keys. Keyboard: digits, operators,\n");
  fprintf (output, "Enter, Backspace, f/g shift; Escape switches power. F10 quits.\n");
}

static void stop_requested (int signum)
{
  (void) signum;
  interrupted = 1;
}

static void display_update (void *ref, int count, segment_bitmap_t *segments)
{
  calculator_t *calculator = ref;

  if (count != NP_DIGITS)
    {
      fatal (2, "unexpected Voyager display size %d\n", count);
    }
  if (memcmp (calculator->segments, segments, sizeof (calculator->segments)))
    {
      memcpy (calculator->segments, segments, sizeof (calculator->segments));
      calculator->display_changed = true;
    }
}

static void draw_mask (calculator_t *calculator, Pixmap mask,
                       int x, int y, int width, int height)
{
  XSetClipMask (calculator->display, calculator->gc, mask);
  XSetClipOrigin (calculator->display, calculator->gc, x, y);
  XFillRectangle (calculator->display, calculator->window, calculator->gc,
                   x, y, width, height);
  XSetClipMask (calculator->display, calculator->gc, None);
}

static void draw_display (calculator_t *calculator)
{
  np_skin_t *skin = &calculator->skin;
  int digit, segment;

  XSetForeground (calculator->display, calculator->gc, skin->background);
  XFillRectangle (calculator->display, calculator->window, calculator->gc,
                   skin->display_x, skin->display_y,
                   skin->display_width, skin->display_height);
  XSetForeground (calculator->display, calculator->gc, skin->foreground);
  for (digit = 0; digit < NP_DIGITS; digit++)
    {
      int x = skin->display_x + skin->digit_x + digit * skin->digit_width;
      int y = skin->display_y + skin->digit_y;

      for (segment = 0; segment < NP_SEGMENTS; segment++)
        {
          if (calculator->segments[digit] & ((segment_bitmap_t) 1 << segment))
            {
              draw_mask (calculator, skin->segments[segment], x, y,
                          skin->digit_width, skin->digit_height);
            }
        }
      if ((calculator->segments[digit] & SEGMENT_ANN) &&
          skin->annunciators[digit].mask)
        {
          np_annunciator_t *ann = &skin->annunciators[digit];

          draw_mask (calculator, ann->mask, skin->display_x + ann->x,
                      skin->display_y + ann->y, ann->width, ann->height);
        }
    }
  calculator->display_changed = false;
}

static void simulate_for (calculator_t *calculator, unsigned milliseconds)
{
  unsigned elapsed;

  for (elapsed = 0; elapsed < milliseconds; elapsed += 10)
    {
      usleep (10000);
      sim_run (calculator->sim);
    }
}

static void tap_key (calculator_t *calculator, int key)
{
  sim_key (calculator->sim, key, true);
  simulate_for (calculator, 120);
  sim_key (calculator->sim, key, false);
  simulate_for (calculator, 120);
}

static int arithmetic_test (calculator_t *calculator)
{
  static const unsigned char digit_segments[10] =
    {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
  char text[NP_DIGITS + 1];
  char *result_digit;
  int index, value;

  /* Exercise the real ROM, key scanner and display driver: 2 ENTER 3 +. */
  simulate_for (calculator, 300);
  tap_key (calculator, 41);
  simulate_for (calculator, 400);
  tap_key (calculator, 35);
  tap_key (calculator, 38);
  tap_key (calculator, 36);
  tap_key (calculator, 39);
  tap_key (calculator, 40);
  simulate_for (calculator, 400);
  sim_get_display_update (calculator->sim);
  for (index = 0; index < NP_DIGITS; index++)
    {
      unsigned bits = calculator->segments[index] & 0x7f;

      text[index] = bits ? '?' : ' ';
      for (value = 0; value < 10; value++)
        {
          if (bits == digit_segments[value])
            {
              text[index] = '0' + value;
              break;
            }
        }
      if (bits == 0x74 || bits == 0x76)
        {
          text[index] = 'h';
        }
    }
  text[NP_DIGITS] = 0;
  printf ("ROM arithmetic display: [%s]\n", text);
  /* The HP-16C right-aligns its integer result and adds a radix indicator. */
  for (index = 0; index < NP_DIGITS; index++)
    {
      if (text[index] != ' ' && text[index] != '0' && text[index] != '5' &&
          !(index == NP_DIGITS - 1 && text[index] == 'h'))
        {
          break;
        }
    }
  result_digit = strchr (text, '5');
  if (index != NP_DIGITS || !result_digit || strchr (result_digit + 1, '5'))
    {
      fprintf (stderr, "Nonpareil: expected 2 ENTER 3 + to display 5\n");
      return 1;
    }
  puts ("Nonpareil arithmetic test: PASS");
  return 0;
}

static void open_window (calculator_t *calculator, const char *display_name,
                         const char *directory)
{
  XSizeHints hints = {0};
  XClassHint class_hint = {"nonpar", "Nonpareil"};
  int screen;

  calculator->display = XOpenDisplay (display_name);
  if (!calculator->display)
    {
      fatal (2, "cannot open X display\n");
    }
  screen = DefaultScreen (calculator->display);
  np_skin_load (calculator->display, screen, directory, &calculator->skin);
  calculator->window = XCreateSimpleWindow (calculator->display,
      RootWindow (calculator->display, screen), 0, 0,
      calculator->skin.width, calculator->skin.height, 0,
      BlackPixel (calculator->display, screen), BlackPixel (calculator->display, screen));
  calculator->gc = XCreateGC (calculator->display, calculator->window, 0, NULL);
  XStoreName (calculator->display, calculator->window, calculator->skin.title);
  XSetClassHint (calculator->display, calculator->window, &class_hint);
  hints.flags = PMinSize | PMaxSize;
  hints.min_width = hints.max_width = calculator->skin.width;
  hints.min_height = hints.max_height = calculator->skin.height;
  XSetWMNormalHints (calculator->display, calculator->window, &hints);
  XSelectInput (calculator->display, calculator->window,
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 KeyPressMask | KeyReleaseMask | FocusChangeMask);
}

static void event_loop (calculator_t *calculator)
{
  Display *display = calculator->display;
  Atom wm_delete = XInternAtom (display, "WM_DELETE_WINDOW", False);

  XSetWMProtocols (display, calculator->window, &wm_delete, 1);
  while (!interrupted)
    {
      while (calculator->keys.count <= NP_KEY_QUEUE_SIZE - 2 &&
             XPending (display) && !interrupted)
        {
          XEvent event;

          XNextEvent (display, &event);
          switch (event.type)
            {
            case Expose:
              XCopyArea (display, calculator->skin.face, calculator->window,
                          calculator->gc, 0, 0, calculator->skin.width,
                          calculator->skin.height, 0, 0);
              calculator->display_changed = true;
              break;
            case ButtonPress:
              if (event.xbutton.button == Button1)
                {
                  np_keys_press (&calculator->keys, np_button_at (&calculator->skin,
                              event.xbutton.x, event.xbutton.y), 0);
                }
              break;
            case ButtonRelease:
              if (event.xbutton.button == Button1 && !calculator->keys.xkey)
                {
                  np_keys_release (&calculator->keys);
                }
              break;
            case KeyPress:
              {
                KeySym symbol;
                char text[8];

                XLookupString (&event.xkey, text, sizeof (text), &symbol, NULL);
                if (symbol == XK_F10)
                  {
                    interrupted = 1;
                  }
                else
                  {
                    np_keys_press (&calculator->keys, np_key_binding (&calculator->skin, symbol),
                                event.xkey.keycode);
                  }
              }
              break;
            case KeyRelease:
              if (event.xkey.keycode == calculator->keys.xkey)
                {
                  np_keys_release (&calculator->keys);
                }
              break;
            case FocusOut:
              np_keys_release (&calculator->keys);
              break;
            case ClientMessage:
              if ((Atom) event.xclient.data.l[0] == wm_delete)
                {
                  interrupted = 1;
                }
              break;
            }
        }
      np_keys_tick (&calculator->keys, calculator->sim);
      sim_run (calculator->sim);
      if (calculator->display_changed)
        {
          draw_display (calculator);
        }
      XFlush (display);
      usleep (10000);
    }
  np_keys_finish (&calculator->keys, calculator->sim);
}

int main (int argc, char **argv)
{
  calculator_t calculator = {0};
  const char *model = "15c";
  const char *data = NP_DATA_DIR;
  const char *display_name = NULL;
  const char *state_filename = NULL;
  char directory[512], definition[512];
  char default_state[512];
  bool test = false;
  bool save_state = true;
  bool restored = false;
  int index, result = 0;

  progname = argv[0];
  np_keys_init (&calculator.keys);
  for (index = 1; index < argc; index++)
    {
      if (!strcmp (argv[index], "-help") || !strcmp (argv[index], "--help"))
        {
          usage (stdout);
          return 0;
        }
      else if (!strcmp (argv[index], "-display") && index + 1 < argc)
        {
          display_name = argv[++index];
        }
      else if (!strcmp (argv[index], "-data") && index + 1 < argc)
        {
          data = argv[++index];
        }
      else if (!strcmp (argv[index], "-test"))
        {
          test = true;
        }
      else if (!strcmp (argv[index], "-state") && index + 1 < argc)
        {
          state_filename = argv[++index];
        }
      else if (!strcmp (argv[index], "-nostate"))
        {
          save_state = false;
        }
      else if (!strcmp (argv[index], "10c") || !strcmp (argv[index], "11c") ||
               !strcmp (argv[index], "12c") || !strcmp (argv[index], "15c") ||
               !strcmp (argv[index], "16c"))
        {
          model = argv[index];
        }
      else
        {
          usage (stderr);
          return 2;
        }
    }
  if (snprintf (directory, sizeof (directory), "%s/%s", data, model) >= sizeof (directory) ||
      snprintf (definition, sizeof (definition), "%s/calc.ncd", directory) >= sizeof (definition))
    {
      fatal (2, "calculator data path is too long\n");
    }
  if (!test)
    {
      open_window (&calculator, display_name, directory);
    }
  if (!state_filename && !test && save_state)
    {
      const char *home = getenv ("HOME");

      if (home && *home)
        {
          if (snprintf (default_state, sizeof (default_state), "%s/np%s.nst",
                         home, model) >= sizeof (default_state))
            {
              fatal (2, "calculator state path is too long\n");
            }
          state_filename = default_state;
        }
    }
  calculator.sim = sim_init (definition, display_update, &calculator);
  sim_reset (calculator.sim);
  if (!test && state_filename && save_state)
    {
      restored = np_state_load (calculator.sim, state_filename, model);
    }
  if (!restored)
    {
      sim_clear_memory (calculator.sim);
    }
  sim_start (calculator.sim);
  signal (SIGINT, stop_requested);
  signal (SIGTERM, stop_requested);
  signal (SIGHUP, stop_requested);
  if (test)
    {
      result = arithmetic_test (&calculator);
    }
  else
    {
      if (!restored)
        {
          simulate_for (&calculator, 300);
          tap_key (&calculator, 41);
          simulate_for (&calculator, 400);
          /* The ROM reports cleared continuous memory on its first boot. */
          tap_key (&calculator, 35);
        }
      XMapWindow (calculator.display, calculator.window);
      event_loop (&calculator);
    }
  sim_stop (calculator.sim);
  if (state_filename && save_state && result == 0)
    {
      if (!np_state_save (calculator.sim, state_filename, model))
        {
          fprintf (stderr, "Nonpareil: cannot save state '%s'\n", state_filename);
          result = 1;
        }
      else if (test)
        {
          segment_bitmap_t expected[NP_DIGITS];

          memcpy (expected, calculator.segments, sizeof (expected));
          sim_reset (calculator.sim);
          sim_clear_memory (calculator.sim);
          if (!np_state_load (calculator.sim, state_filename, model) ||
              memcmp (expected, calculator.segments, sizeof (expected)))
            {
              fprintf (stderr, "Nonpareil: state round trip changed the display\n");
              result = 1;
            }
          else
            {
              puts ("Nonpareil state round trip: PASS");
            }
        }
    }
  sim_quit (calculator.sim);
  if (calculator.display)
    {
      np_skin_free (calculator.display, &calculator.skin);
      XFreeGC (calculator.display, calculator.gc);
      XDestroyWindow (calculator.display, calculator.window);
      XCloseDisplay (calculator.display);
    }
  return result;
}
