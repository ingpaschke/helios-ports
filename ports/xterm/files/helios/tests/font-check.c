/* SPDX-License-Identifier: MIT */
/* Compare our streaming loader with unmodified Xlib on the same X server. */
#include "font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslib.h>
#include <nonansi.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int equal(const XCharStruct *a,const XCharStruct *b)
{return a && b && a->lbearing==b->lbearing && a->rbearing==b->rbearing && a->width==b->width && a->ascent==b->ascent && a->descent==b->descent && a->attributes==b->attributes;}
int main(int argc,char **argv)
{
    static const char *names[]={
        "-misc-fixed-medium-r-normal--18-120-100-100-c-90-iso10646-1",
        "-misc-fixed-bold-r-normal--18-120-100-100-c-90-iso10646-1",
        "-misc-fixed-medium-r-normal-ja-18-120-100-100-c-180-iso10646-1",
        "9x15", "variable"};
    Display *d;unsigned f,i,n,j;XFontStruct *plain,*small;long before,after;
    CHECK(argc==2);d=XOpenDisplay(argv[1]);CHECK(d);
    for(f=0;f<sizeof(names)/sizeof(*names);f++){
        char bytes[256];XChar2b text[64];
        plain=XLoadQueryFont(d,(char *)names[f]);small=helios_font_load(d,names[f]);
        CHECK(plain && small);CHECK(small->per_char==NULL);
        CHECK(plain->direction==small->direction && plain->min_char_or_byte2==small->min_char_or_byte2 && plain->max_char_or_byte2==small->max_char_or_byte2 && plain->min_byte1==small->min_byte1 && plain->max_byte1==small->max_byte1 && plain->default_char==small->default_char && plain->all_chars_exist==small->all_chars_exist && plain->ascent==small->ascent && plain->descent==small->descent);
        CHECK(equal(&plain->min_bounds,&small->min_bounds) && equal(&plain->max_bounds,&small->max_bounds));
        CHECK(plain->n_properties==small->n_properties);
        for(i=0;i<(unsigned)plain->n_properties;i++)CHECK(plain->properties[i].name==small->properties[i].name && plain->properties[i].card32==small->properties[i].card32);
        n=(plain->max_char_or_byte2-plain->min_char_or_byte2+1)*(plain->max_byte1-plain->min_byte1+1);
        CHECK(!!plain->per_char==!!helios_font_has_metrics(small));
        if(plain->per_char)for(i=0;i<n;i++)CHECK(equal(plain->per_char+i,helios_font_char(small,i)));
        for(i=0;i<256;i++)bytes[i]=(char)i;
        CHECK(XTextWidth(plain,bytes,256)==helios_font_width(small,bytes,256));
        for(j=0;j<128;j++){
            for(i=0;i<64;i++){unsigned ch=(j*487+i*977)&65535;text[i].byte1=ch>>8;text[i].byte2=ch;}
            CHECK(XTextWidth16(plain,text,64)==helios_font_width16(small,text,64));
        }
        printf("PASS font %s: %u metrics, cache %lu bytes\n",names[f],plain->per_char?n:0,(unsigned long)helios_font_cache_bytes(small));fflush(stdout);
        CHECK(helios_font_width(plain,"fallback",8)==XTextWidth(plain,"fallback",8));
        helios_font_free(d,small);XFreeFont(d,plain);
    }
    CHECK(!helios_font_load(d,"helios-no-such-font-123456"));XSync(d,0);
    /* Warm up atom/font bookkeeping, then check repeated ownership cleanup. */
    small=helios_font_load(d,names[0]);CHECK(small);helios_font_free(d,small);XSync(d,0);
    before=(long)Malloc(-1);
    for(i=0;i<20;i++){small=helios_font_load(d,names[0]);CHECK(small);CHECK(helios_font_cache_bytes(small)<393216);helios_font_free(d,small);}
    XSync(d,0);after=(long)Malloc(-1);CHECK(after>=before-1024);
    XCloseDisplay(d);puts("PASS font ownership: missing font, continued protocol use and 20 load/free cycles");return 0;
}
