/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NP_XML_READER_H
#define NP_XML_READER_H

typedef unsigned char xmlChar;

typedef struct
{
  void (*startElement) (void *, const xmlChar *, const xmlChar **);
  void (*endElement) (void *, const xmlChar *);
  void (*characters) (void *, const xmlChar *, int);
} np_xml_handlers_t;

int xml_strcmp (const xmlChar *left, const char *right);
void np_xml_read (np_xml_handlers_t *handlers, void *ref, const char *filename);

#endif
