/* SPDX-License-Identifier: GPL-2.0-only */
#include <expat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "xml_reader.h"

typedef struct
{
  np_xml_handlers_t *handlers;
  void *ref;
  char text[4096];
  size_t text_length;
} xml_reader_t;

int xml_strcmp (const xmlChar *left, const char *right)
{
  return strcmp ((const char *) left, right);
}

static void flush_text (xml_reader_t *reader)
{
  if (reader->text_length && reader->handlers->characters)
    {
      /* The character-generator handlers consume one complete element.
         Expat may split that element across input buffers or entities. */
      reader->handlers->characters (reader->ref,
                                   (const xmlChar *) reader->text,
                                   reader->text_length);
    }
  reader->text_length = 0;
}

static void start_element (void *ref, const char *name, const char **attributes)
{
  xml_reader_t *reader = ref;

  flush_text (reader);
  reader->handlers->startElement (reader->ref, (const xmlChar *) name,
                                 (const xmlChar **) attributes);
}

static void end_element (void *ref, const char *name)
{
  xml_reader_t *reader = ref;

  flush_text (reader);
  reader->handlers->endElement (reader->ref, (const xmlChar *) name);
}

static void character_data (void *ref, const char *text, int length)
{
  xml_reader_t *reader = ref;

  if (!reader->handlers->characters)
    {
      return;
    }
  if ((size_t) length > sizeof (reader->text) - reader->text_length)
    {
      fatal (2, "calculator definition: character data is too long\n");
    }
  memcpy (reader->text + reader->text_length, text, length);
  reader->text_length += length;
}

void np_xml_read (np_xml_handlers_t *handlers, void *ref, const char *filename)
{
  xml_reader_t reader = {0};
  XML_Parser parser;
  FILE *input;
  char buffer[8192];
  size_t count;
  int complete;

  input = fopen (filename, "rb");
  if (!input)
    {
      fatal (2, "cannot open calculator definition '%s'\n", filename);
    }
  parser = XML_ParserCreate (NULL);
  if (!parser)
    {
      fclose (input);
      fatal (2, "cannot allocate XML parser\n");
    }
  reader.handlers = handlers;
  reader.ref = ref;
  handlers->characters = NULL;
  XML_SetUserData (parser, &reader);
  XML_SetElementHandler (parser, start_element, end_element);
  XML_SetCharacterDataHandler (parser, character_data);
  do
    {
      count = fread (buffer, 1, sizeof (buffer), input);
      complete = count < sizeof (buffer);
      if (ferror (input))
        {
          XML_ParserFree (parser);
          fclose (input);
          fatal (2, "error reading calculator definition '%s'\n", filename);
        }
      if (XML_Parse (parser, buffer, count, complete) == XML_STATUS_ERROR)
        {
          fprintf (stderr, "%s:%lu: %s\n", filename,
                   (unsigned long) XML_GetCurrentLineNumber (parser),
                   XML_ErrorString (XML_GetErrorCode (parser)));
          XML_ParserFree (parser);
          fclose (input);
          exit (2);
        }
    }
  while (!complete);
  XML_ParserFree (parser);
  fclose (input);
}
