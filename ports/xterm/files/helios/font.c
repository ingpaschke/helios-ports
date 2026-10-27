/* QueryFont transport adapted from MIT X11R4 lib/X/XFont.c.
 * Copyright 1986 Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission. M.I.T. makes no representations about the
 * suitability of this software for any purpose. It is provided "as is"
 * without express or implied warranty.
 *
 * xterm-only font loading: consume metrics a page at a time, retaining their
 * exact values in a compact cache. No public Xlib layout or library changes.
 */
#define NEED_REPLIES
/* Native headers use BYTE as a signed-byte macro; X11 defines its own type. */
#undef BYTE
#include <X11/Xlibint.h>
#include "font.h"
#include "font-cache.h"
#include <stdlib.h>
#include <string.h>
#undef XLoadQueryFont
#undef XFreeFont
#undef XTextWidth
#undef XTextWidth16
_Static_assert(sizeof(XCharStruct)==12,"QueryFont metrics must match X11 wire data");
typedef struct FontCache {
    struct FontCache *next;
    XFontStruct *font;
    HFontCache *cache;
} FontCache;
static FontCache *fonts;
static FontCache *lookup(const XFontStruct *font)
{FontCache *p;for(p=fonts;p;p=p->next)if(p->font==font)return p;return NULL;}
int helios_font_has_metrics(const XFontStruct *font)
{FontCache *p=lookup(font);return p?p->cache!=NULL:font->per_char!=NULL;}
const XCharStruct *helios_font_char(const XFontStruct *font,unsigned index)
{FontCache *p=lookup(font);return p?hfont_cache_get(p->cache,index):(font->per_char?font->per_char+index:NULL);}
size_t helios_font_cache_bytes(const XFontStruct *font)
{FontCache *p=lookup(font);return p?sizeof(*p)+hfont_cache_bytes(p->cache):0;}

static int query_font(Display *dpy,Font fid,XFontStruct **result)
{
    xResourceReq *req;xQueryFontReply reply;XFontStruct *fs=NULL;
    FontCache *entry=NULL;unsigned first,count,n;unsigned long remaining;
    XCharStruct metrics[256];
    GetResReq(QueryFont,fid,req);
    if(!_XReply(dpy,(xReply *)&reply,
               (SIZEOF(xQueryFontReply)-SIZEOF(xReply))>>2,xFalse))return 1;
    remaining=(unsigned long)reply.nFontProps*SIZEOF(xFontProp)+
              (unsigned long)reply.nCharInfos*SIZEOF(xCharInfo);
    fs=calloc(1,sizeof(*fs));entry=calloc(1,sizeof(*entry));
    if(!fs || !entry)goto failed;
    fs->fid=fid;fs->direction=reply.drawDirection;
    fs->min_char_or_byte2=reply.minCharOrByte2;fs->max_char_or_byte2=reply.maxCharOrByte2;
    fs->min_byte1=reply.minByte1;fs->max_byte1=reply.maxByte1;
    fs->default_char=reply.defaultChar;fs->all_chars_exist=reply.allCharsExist;
    fs->ascent=cvtINT16toInt(reply.fontAscent);fs->descent=cvtINT16toInt(reply.fontDescent);
#define BOUND(dst,src) do { \
    fs->dst.lbearing=cvtINT16toInt(reply.src.leftSideBearing); \
    fs->dst.rbearing=cvtINT16toInt(reply.src.rightSideBearing); \
    fs->dst.width=cvtINT16toInt(reply.src.characterWidth); \
    fs->dst.ascent=cvtINT16toInt(reply.src.ascent); \
    fs->dst.descent=cvtINT16toInt(reply.src.descent); \
    fs->dst.attributes=reply.src.attributes; \
} while(0)
    BOUND(min_bounds,minBounds);BOUND(max_bounds,maxBounds);
#undef BOUND
    fs->n_properties=reply.nFontProps;
    if(fs->n_properties){
        fs->properties=malloc(fs->n_properties*sizeof(*fs->properties));
        if(!fs->properties)goto failed;
        n=fs->n_properties*SIZEOF(xFontProp);
        _XRead32(dpy,(char *)fs->properties,n);remaining-=n;
    }
    count=reply.nCharInfos;
    if(count){
        if(fs->min_char_or_byte2>fs->max_char_or_byte2 || fs->min_byte1>fs->max_byte1 ||
           fs->max_char_or_byte2>255 || fs->max_byte1>255 ||
           count!=(fs->max_char_or_byte2-fs->min_char_or_byte2+1)*
                  (fs->max_byte1-fs->min_byte1+1))goto failed;
        entry->cache=hfont_cache_new(count);if(!entry->cache)goto failed;
        for(first=0;first<count;first+=n){
            n=count-first;if(n>256)n=256;
            _XRead16(dpy,(char *)metrics,n*SIZEOF(xCharInfo));remaining-=n*SIZEOF(xCharInfo);
            if(!hfont_cache_page(entry->cache,first,metrics,n))goto failed;
        }
    }
    entry->font=fs;entry->next=fonts;fonts=entry;*result=fs;
    {_XExtension *ext;
     for(ext=dpy->ext_procs;ext;ext=ext->next)
        if(ext->create_Font)(*ext->create_Font)(dpy,fs,&ext->codes);}
    return 0;
failed:
    _XEatData(dpy,remaining);
    if(entry){hfont_cache_free(entry->cache);free(entry);}
    if(fs){free(fs->properties);free(fs);}
    return 2;
}
XFontStruct *helios_font_load(Display *dpy,const char *name)
{
    XFontStruct *font=NULL;xOpenFontReq *req;long nbytes;Font fid;int error;
    LockDisplay(dpy);
    GetReq(OpenFont,req);nbytes=req->nbytes=name?strlen(name):0;
    req->fid=fid=XAllocID(dpy);req->length+=(nbytes+3)>>2;Data(dpy,name,nbytes);
    /* Attribute a failed OpenFont to this query, as in the X11R4 loader. */
    dpy->request--;error=query_font(dpy,fid,&font);dpy->request++;
    if(error==1){xReply reply;(void)_XReply(dpy,&reply,0,xFalse);}
    else if(error==2){xResourceReq *req;GetResReq(CloseFont,fid,req);}
    UnlockDisplay(dpy);SyncHandle();return font;
}
int helios_font_free(Display *dpy,XFontStruct *font)
{
    FontCache **slot=&fonts,*p;
    while(*slot && (*slot)->font!=font)slot=&(*slot)->next;
    if((p=*slot)!=NULL){*slot=p->next;hfont_cache_free(p->cache);free(p);}
    /* per_char remains NULL for our fonts. Ordinary Xt fonts remain valid. */
    return XFreeFont(dpy,font);
}
static int nonexistent(const XCharStruct *m)
{return !m || (!m->width && !(m->lbearing|m->rbearing));}
static const XCharStruct *glyph(const XFontStruct *fs,unsigned ch,int rowzero)
{
    unsigned row=ch>>8,col=ch&255,index;const XCharStruct *m;
    if(!fs->max_byte1){
        if(ch<fs->min_char_or_byte2 || ch>fs->max_char_or_byte2)return NULL;
        index=ch-fs->min_char_or_byte2;
    }else{
        if(rowzero)row=0;
        if(row<fs->min_byte1 || row>fs->max_byte1 ||
           col<fs->min_char_or_byte2 || col>fs->max_char_or_byte2)return NULL;
        index=(row-fs->min_byte1)*(fs->max_char_or_byte2-fs->min_char_or_byte2+1)+col-fs->min_char_or_byte2;
    }
    m=helios_font_has_metrics(fs)?helios_font_char(fs,index):&fs->min_bounds;
    return nonexistent(m)?NULL:m;
}
int helios_font_width(XFontStruct *font,const char *text,int count)
{
    const XCharStruct *def,*m;int i,width=0;
    if(!lookup(font))return XTextWidth(font,(char *)text,count);
    def=glyph(font,font->default_char,0);
    for(i=0;i<count;i++){
        m=glyph(font,(unsigned char)text[i],1);if(!m)m=def;
        if(m)width+=m->width;
    }
    return width;
}
int helios_font_width16(XFontStruct *font,const XChar2b *text,int count)
{
    const XCharStruct *def,*m;int i,width=0;
    if(!lookup(font))return XTextWidth16(font,(XChar2b *)text,count);
    def=glyph(font,font->default_char,0);
    for(i=0;i<count;i++){
        m=glyph(font,((unsigned)text[i].byte1<<8)|text[i].byte2,0);if(!m)m=def;
        if(m)width+=m->width;
    }
    return width;
}
