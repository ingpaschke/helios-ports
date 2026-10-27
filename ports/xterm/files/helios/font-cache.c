/* SPDX-License-Identifier: MIT */
/* Lossless, constant-time lookup. Empty pages use no allocation; other pages
 * use one common metric, byte indices into a palette, or uncompressed data. */
#include "font-cache.h"
#include <stdlib.h>
#include <string.h>
#define PAGE 256u
#define RAW 65535u
typedef struct {
    unsigned short count,kind;
    unsigned char data[];
} FontPage;
struct HFontCache { unsigned count,pages;size_t bytes;FontPage *page[]; };
static const XCharStruct empty;
static unsigned aligned(unsigned n){return (n+3u)&~3u;}
static int equal(const XCharStruct *a,const XCharStruct *b)
{
    return a->lbearing==b->lbearing && a->rbearing==b->rbearing &&
           a->width==b->width && a->ascent==b->ascent &&
           a->descent==b->descent && a->attributes==b->attributes;
}
HFontCache *hfont_cache_new(unsigned count)
{
    HFontCache *c;unsigned pages;
    if(!count || count>65536u)return NULL;
    pages=(count+PAGE-1)/PAGE;
    c=calloc(1,sizeof(*c)+pages*sizeof(c->page[0]));
    if(c){c->count=count;c->pages=pages;c->bytes=sizeof(*c)+pages*sizeof(c->page[0]);}
    return c;
}
int hfont_cache_page(HFontCache *c,unsigned first,const XCharStruct *metrics,unsigned count)
{
    XCharStruct palette[PAGE];unsigned char indices[PAGE];
    unsigned i,j,distinct=0,kind;size_t bytes;FontPage *p;
    if(!c || !metrics || first%PAGE || first>=c->count ||
       count!=(c->count-first<PAGE?c->count-first:PAGE) || c->page[first/PAGE])return 0;
    for(i=0;i<count;i++){
        for(j=0;j<distinct;j++)if(equal(metrics+i,palette+j))break;
        if(j==distinct)palette[distinct++]=metrics[i];
        indices[i]=(unsigned char)j;
    }
    if(distinct==1 && equal(palette,&empty))return 1;
    kind=distinct;
    bytes=sizeof(*p)+(kind==1?sizeof(XCharStruct):aligned(count)+distinct*sizeof(XCharStruct));
    if(kind>1 && bytes>=sizeof(*p)+count*sizeof(XCharStruct)){
        kind=RAW;bytes=sizeof(*p)+count*sizeof(XCharStruct);
    }
    p=malloc(bytes);if(!p)return 0;
    p->count=(unsigned short)count;p->kind=(unsigned short)kind;
    if(kind==RAW)memcpy(p->data,metrics,count*sizeof(XCharStruct));
    else if(kind==1)memcpy(p->data,palette,sizeof(XCharStruct));
    else{
        memcpy(p->data,indices,count);
        memcpy(p->data+aligned(count),palette,distinct*sizeof(XCharStruct));
    }
    c->page[first/PAGE]=p;c->bytes+=bytes;return 1;
}
const XCharStruct *hfont_cache_get(const HFontCache *c,unsigned index)
{
    const FontPage *p;unsigned cell;
    if(!c || index>=c->count)return NULL;
    p=c->page[index/PAGE];if(!p)return &empty;
    cell=index%PAGE;
    if(p->kind==RAW)return ((const XCharStruct *)(const void *)p->data)+cell;
    if(p->kind==1)return (const XCharStruct *)(const void *)p->data;
    return ((const XCharStruct *)(const void *)(p->data+aligned(p->count)))+p->data[cell];
}
size_t hfont_cache_bytes(const HFontCache *c){return c?c->bytes:0;}
void hfont_cache_free(HFontCache *c)
{
    unsigned i;if(!c)return;
    for(i=0;i<c->pages;i++)free(c->page[i]);
    free(c);
}
