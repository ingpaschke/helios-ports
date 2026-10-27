/* SPDX-License-Identifier: MIT */
#include "font-cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long fail_after=-1,live;
void *test_malloc(size_t n){void *p;if(fail_after==0)return NULL;if(fail_after>0)--fail_after;p=malloc(n);if(p)++live;return p;}
void *test_calloc(size_t n,size_t s){void *p;if(fail_after==0)return NULL;if(fail_after>0)--fail_after;p=calloc(n,s);if(p)++live;return p;}
void test_free(void *p){if(p)--live;free(p);}
static XCharStruct glyph(unsigned i)
{
    XCharStruct m={0};m.lbearing=(short)(i%19)-9;m.rbearing=(short)(i%41);
    m.width=(short)(i%3)*9;m.ascent=(short)(i%17);m.descent=(short)(i%5);
    m.attributes=(unsigned short)i;return m;
}
static void check(const XCharStruct *a,const XCharStruct *b)
{assert(a && b);assert(a->lbearing==b->lbearing && a->rbearing==b->rbearing && a->width==b->width && a->ascent==b->ascent && a->descent==b->descent && a->attributes==b->attributes);}
int main(void)
{
    HFontCache *c;XCharStruct page[256],zero={0};unsigned i,p;
    assert(!hfont_cache_new(0));assert(!hfont_cache_new(65537));
    fail_after=0;assert(!hfont_cache_new(256));assert(!live);fail_after=-1;
    c=hfont_cache_new(65536);assert(c);
    memset(page,0,sizeof(page));
    for(p=0;p<65536;p+=256)assert(hfont_cache_page(c,p,page,256));
    for(i=0;i<65536;i++)check(hfont_cache_get(c,i),&zero);
    assert(hfont_cache_bytes(c)<4096);assert(!hfont_cache_get(c,65536));
    hfont_cache_free(c);assert(!live);
    for(p=0;p<4;p++){
        unsigned count=p==3?253:256;c=hfont_cache_new(count);assert(c);
        for(i=0;i<count;i++)page[i]=glyph(p==0?7:p==1?i%5:i);
        assert(!hfont_cache_page(c,1,page,count));
        fail_after=0;assert(!hfont_cache_page(c,0,page,count));fail_after=-1;
        assert(hfont_cache_page(c,0,page,count));
        assert(!hfont_cache_page(c,0,page,count));
        for(i=0;i<count;i++)check(hfont_cache_get(c,i),page+i);
        assert(!hfont_cache_get(c,count));hfont_cache_free(c);assert(!live);
    }
    c=hfont_cache_new(65535);assert(c);
    for(p=0;p<65535;p+=256){
        unsigned count=65535-p<256?65535-p:256;
        for(i=0;i<count;i++)page[i]=glyph((p+i)%43);
        assert(hfont_cache_page(c,p,page,count));
    }
    for(i=0;i<65535;i++){XCharStruct m=glyph(i%43);check(hfont_cache_get(c,i),&m);}
    hfont_cache_free(c);assert(!live);
    puts("PASS compact font cache: empty, uniform, palette, raw, partial pages, all metrics, boundaries and allocation failures");return 0;
}
