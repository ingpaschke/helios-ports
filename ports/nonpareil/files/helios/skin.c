/* SPDX-License-Identifier: GPL-2.0-only */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "skin.h"

static void asset_path (char *path, size_t size, const char *directory,
                        const char *filename)
{
  if (snprintf (path, size, "%s/%s", directory, filename) >= (int) size)
    {
      fatal (2, "calculator data path is too long\n");
    }
}

static unsigned long color (Display *display, int screen, int r, int g, int b)
{
  XColor value;

  value.red = r * 257;
  value.green = g * 257;
  value.blue = b * 257;
  value.flags = DoRed | DoGreen | DoBlue;
  if (!XAllocColor (display, DefaultColormap (display, screen), &value))
    {
      fatal (2, "cannot allocate calculator color\n");
    }
  return value.pixel;
}

static unsigned long component (unsigned value, unsigned long mask)
{
  unsigned shift = 0;

  if (!mask)
    {
      return 0;
    }
  while (!(mask & 1))
    {
      mask >>= 1;
      shift++;
    }
  return ((value * mask + 127) / 255) << shift;
}

static Pixmap load_face (Display *display, int screen, const char *filename,
                         int width, int height)
{
  FILE *input;
  XImage *image;
  Visual *visual = DefaultVisual (display, screen);
  unsigned long palette[64];
  unsigned char rgb[3];
  unsigned index;
  int x, y, file_width, file_height, maximum;
  Pixmap pixmap;
  GC gc;

  input = fopen (filename, "rb");
  if (!input || fscanf (input, "P6 %d %d %d", &file_width, &file_height,
                         &maximum) != 3 || fgetc (input) != '\n' ||
      file_width != width || file_height != height || maximum != 255)
    {
      fatal (2, "invalid calculator face '%s'\n", filename);
    }
  image = XCreateImage (display, visual, DefaultDepth (display, screen),
                        ZPixmap, 0, NULL, width, height, 32, 0);
  if (!image)
    {
      fclose (input);
      fatal (2, "cannot create calculator image\n");
    }
  image->data = calloc (height, image->bytes_per_line);
  if (!image->data)
    {
      fclose (input);
      XDestroyImage (image);
      fatal (2, "cannot allocate calculator image\n");
    }
  if (visual->class != TrueColor)
    {
      /* Leave colormap entries available for the window manager and LCD. */
      for (index = 0; index < 64; index++)
        {
          palette[index] = color (display, screen,
                                  ((index >> 4) & 3) * 255 / 3,
                                  ((index >> 2) & 3) * 255 / 3,
                                  (index & 3) * 255 / 3);
        }
    }
  for (y = 0; y < height; y++)
    {
      for (x = 0; x < width; x++)
        {
          unsigned long pixel;

          if (fread (rgb, 1, sizeof (rgb), input) != sizeof (rgb))
            {
              fclose (input);
              XDestroyImage (image);
              fatal (2, "truncated calculator face '%s'\n", filename);
            }
          if (visual->class == TrueColor)
            {
              pixel = component (rgb[0], visual->red_mask) |
                      component (rgb[1], visual->green_mask) |
                      component (rgb[2], visual->blue_mask);
            }
          else
            {
              index = ((rgb[0] >> 2) & 0x30) | ((rgb[1] >> 4) & 0x0c) |
                      (rgb[2] >> 6);
              pixel = palette[index];
            }
          XPutPixel (image, x, y, pixel);
        }
    }
  fclose (input);
  pixmap = XCreatePixmap (display, RootWindow (display, screen), width, height,
                          DefaultDepth (display, screen));
  gc = XCreateGC (display, pixmap, 0, NULL);
  XPutImage (display, pixmap, gc, image, 0, 0, 0, 0, width, height);
  XFreeGC (display, gc);
  XDestroyImage (image);
  return pixmap;
}

static Pixmap load_mask (Display *display, int screen, const char *directory,
                         const char *filename, int width, int height)
{
  char path[512];
  unsigned actual_width, actual_height;
  int hotspot_x, hotspot_y;
  Pixmap mask;

  asset_path (path, sizeof (path), directory, filename);
  if (XReadBitmapFile (display, RootWindow (display, screen), path,
                       &actual_width, &actual_height, &mask,
                       &hotspot_x, &hotspot_y) != BitmapSuccess)
    {
      fatal (2, "cannot read calculator mask '%s'\n", path);
    }
  if (actual_width != (unsigned) width || actual_height != (unsigned) height)
    {
      fatal (2, "wrong calculator mask dimensions: '%s'\n", path);
    }
  return mask;
}

void np_skin_load (Display *display, int screen, const char *directory,
                   np_skin_t *skin)
{
  char path[512], line[256], name[64];
  FILE *input;
  int r, g, b, index, digits;
  unsigned line_number = 0;

  asset_path (path, sizeof (path), directory, "skin.txt");
  input = fopen (path, "r");
  if (!input)
    {
      fatal (2, "cannot read calculator skin '%s'\n", path);
    }
  while (fgets (line, sizeof (line), input))
    {
      int parsed = 0;

      line_number++;
      if (line[0] == '#' || line[0] == '\n')
        {
          continue;
        }
      if (strncmp (line, "title ", 6) == 0)
        {
          snprintf (skin->title, sizeof (skin->title), "%.127s", line + 6);
          skin->title[strcspn (skin->title, "\r\n")] = 0;
          parsed = 1;
        }
      else if (sscanf (line, "size %d %d", &skin->width, &skin->height) == 2)
        {
          parsed = 1;
        }
      else if (sscanf (line, "display %d %d %d %d", &skin->display_x,
                       &skin->display_y, &skin->display_width,
                       &skin->display_height) == 4)
        {
          parsed = 1;
        }
      else if (sscanf (line, "digits %d %d %d %d %d", &digits,
                       &skin->digit_width, &skin->digit_height,
                       &skin->digit_x, &skin->digit_y) == 5 && digits == NP_DIGITS)
        {
          parsed = 1;
        }
      else if (sscanf (line, "background %d %d %d", &r, &g, &b) == 3)
        {
          skin->background = color (display, screen, r, g, b);
          parsed = 1;
        }
      else if (sscanf (line, "foreground %d %d %d", &r, &g, &b) == 3)
        {
          skin->foreground = color (display, screen, r, g, b);
          parsed = 1;
        }
      else if (strncmp (line, "button ", 7) == 0 &&
               skin->button_count < NP_MAX_BUTTONS)
        {
          np_button_t *button = &skin->buttons[skin->button_count];

          if (sscanf (line + 7, "%d %d %d %d %d", &button->code, &button->x,
                       &button->y, &button->width, &button->height) == 5)
            {
              skin->button_count++;
              parsed = 1;
            }
        }
      else if (sscanf (line, "key %63s %d", name, &index) == 2 &&
               skin->binding_count < NP_MAX_BINDINGS)
        {
          np_binding_t *binding = &skin->bindings[skin->binding_count];

          binding->symbol = XStringToKeysym (name);
          binding->code = index;
          if (binding->symbol != NoSymbol)
            {
              skin->binding_count++;
              parsed = 1;
            }
        }
      else if (sscanf (line, "annunciator %d", &index) == 1 &&
               index > 0 && index < NP_DIGITS)
        {
          np_annunciator_t *ann = &skin->annunciators[index];

          if (sscanf (line, "annunciator %d %d %d %d %d", &index,
                       &ann->x, &ann->y, &ann->width, &ann->height) == 5)
            {
              snprintf (name, sizeof (name), "ann%d.xbm", index);
              ann->mask = load_mask (display, screen, directory, name,
                                      ann->width, ann->height);
              parsed = 1;
            }
        }
      if (!parsed)
        {
          fatal (2, "%s:%u: invalid skin record\n", path, line_number);
        }
    }
  fclose (input);
  if (skin->width <= 0 || skin->width > 2048 || skin->height <= 0 ||
      skin->height > 2048 || skin->digit_width != 27 || skin->digit_height != 31)
    {
      fatal (2, "invalid calculator dimensions\n");
    }
  asset_path (path, sizeof (path), directory, "face.ppm");
  skin->face = load_face (display, screen, path, skin->width, skin->height);
  for (index = 0; index < NP_SEGMENTS; index++)
    {
      snprintf (name, sizeof (name), "seg%c.xbm", 'a' + index);
      skin->segments[index] = load_mask (display, screen, directory, name,
                                         skin->digit_width, skin->digit_height);
    }
}

void np_skin_free (Display *display, np_skin_t *skin)
{
  int index;

  if (skin->face)
    {
      XFreePixmap (display, skin->face);
    }
  for (index = 0; index < NP_SEGMENTS; index++)
    {
      if (skin->segments[index])
        {
          XFreePixmap (display, skin->segments[index]);
        }
    }
  for (index = 0; index < NP_DIGITS; index++)
    {
      if (skin->annunciators[index].mask)
        {
          XFreePixmap (display, skin->annunciators[index].mask);
        }
    }
}

int np_button_at (const np_skin_t *skin, int x, int y)
{
  int index;

  for (index = 0; index < skin->button_count; index++)
    {
      const np_button_t *button = &skin->buttons[index];

      if (x >= button->x && x < button->x + button->width &&
          y >= button->y && y < button->y + button->height)
        {
          return button->code;
        }
    }
  return -1;
}

int np_key_binding (const np_skin_t *skin, KeySym symbol)
{
  int index;

  for (index = 0; index < skin->binding_count; index++)
    {
      if (symbol == skin->bindings[index].symbol)
        {
          return skin->bindings[index].code;
        }
    }
  return -1;
}
