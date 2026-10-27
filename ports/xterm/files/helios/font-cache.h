/* SPDX-License-Identifier: MIT */
#ifndef HELIOS_FONT_CACHE_H
#define HELIOS_FONT_CACHE_H
#include <stddef.h>
#include <X11/Xlib.h>
typedef struct HFontCache HFontCache;
HFontCache *hfont_cache_new(unsigned count);
int hfont_cache_page(HFontCache *,unsigned first,const XCharStruct *,unsigned count);
const XCharStruct *hfont_cache_get(const HFontCache *,unsigned index);
size_t hfont_cache_bytes(const HFontCache *);
void hfont_cache_free(HFontCache *);
#endif
