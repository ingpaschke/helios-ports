/* SPDX-License-Identifier: GPL-2.0-only */
#include <ctype.h>
#include <errno.h>
#include <stdint.h>

/* The native C library has only 32-bit strtoul. Calculator registers and
   character-generator bitmaps can occupy up to 64 bits. */
unsigned long long np_strtoull (const char *text, char **end, int base)
{
  const char *start = text;
  uint64_t value = 0;
  unsigned digit;
  int negative = 0;
  int overflow = 0;
  int converted = 0;

  if (base != 0 && (base < 2 || base > 36))
    {
      errno = EINVAL;
      if (end)
        {
          *end = (char *) start;
        }
      return 0;
    }
  while (isspace ((unsigned char) *text))
    {
      text++;
    }
  if (*text == '-' || *text == '+')
    {
      negative = *text == '-';
      text++;
    }
  if (*text == '0' && (text[1] == 'x' || text[1] == 'X') &&
      isxdigit ((unsigned char) text[2]) && (base == 0 || base == 16))
    {
      base = 16;
      text += 2;
    }
  if (base == 0)
    {
      base = *text == '0' ? 8 : 10;
    }
  for (;; text++)
    {
      unsigned char ch = (unsigned char) *text;

      if (ch >= '0' && ch <= '9')
        {
          digit = ch - '0';
        }
      else if (ch >= 'a' && ch <= 'z')
        {
          digit = ch - 'a' + 10;
        }
      else if (ch >= 'A' && ch <= 'Z')
        {
          digit = ch - 'A' + 10;
        }
      else
        {
          break;
        }
      if (digit >= (unsigned) base)
        {
          break;
        }
      converted = 1;
      if (value > (UINT64_MAX - digit) / base)
        {
          overflow = 1;
        }
      else
        {
          value = value * base + digit;
        }
    }
  if (end)
    {
      *end = (char *) (converted ? text : start);
    }
  if (overflow)
    {
      errno = ERANGE;
      return UINT64_MAX;
    }
  if (negative)
    {
      value = -value;
    }
  return value;
}
