/* SPDX-License-Identifier: MIT */
/* X11R4 selection conversion for the native single-byte locale. */
#include <xterm.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
_Static_assert(sizeof(XChar2b)==2,"XChar2b must match the two-byte X11 wire encoding");
int XmbTextPropertyToTextList(Display *d,const XTextProperty *p,char ***v,int *n)
{
    (void)d;
    if(p->encoding!=XA_STRING || p->format!=8)return XConverterNotFound;
    return XTextPropertyToStringList((XTextProperty *)p,v,n)?0:XNoMemory;
}
int XmbTextListToTextProperty(Display *d,char **v,int n,XICCEncodingStyle style,XTextProperty *p)
{
    (void)d;
    if(style!=XStringStyle && style!=XStdICCTextStyle && style!=XCompoundTextStyle)return XConverterNotFound;
    return XStringListToTextProperty(v,n,p)?0:XNoMemory;
}

void XmuCvtStringToGravity(XrmValuePtr args,Cardinal *n,XrmValuePtr from,XrmValuePtr to)
{
    static int gravity;
    static const char *names[]={"Forget","NorthWest","North","NorthEast","West","Center","East","SouthWest","South","SouthEast","Static"};
    char name[32]; unsigned i,j;
    (void)args;(void)n;to->addr=NULL;to->size=0;
    if(!from->addr || strlen(from->addr)>=sizeof(name))return;
    strcpy(name,from->addr);
    for(i=0;name[i];i++)if(name[i]>='A'&&name[i]<='Z')name[i]+='a'-'A';
    for(i=0;i<sizeof(names)/sizeof(*names);i++) {
        for(j=0;names[i][j] && name[j]==(names[i][j]>='A'&&names[i][j]<='Z'?names[i][j]+'a'-'A':names[i][j]);j++);
        if(!names[i][j] && (!name[j] || !strcmp(name+j,"gravity"))) {
            gravity=i;to->addr=(XPointer)&gravity;to->size=sizeof(gravity);return;
        }
    }
}

Bool XSupportsLocale(void) { return True; }
Status XInternAtoms(Display *d,char **names,int count,Bool only,Atom *atoms)
{
    int i;for(i=0;i<count;i++)atoms[i]=XInternAtom(d,names[i],only);return 1;
}

#undef XParseColor
Status helios_parse_color(Display *d,Colormap cmap,const char *spec,XColor *color)
{
    char hex[14],part[5],*end;unsigned i,n,value;const char *p;
    if(strncmp(spec,"rgb:",4))return XParseColor(d,cmap,spec,color);
    p=spec+4;hex[0]='#';
    for(i=0;i<3;i++) {
        n=0;while(p[n] && p[n]!='/' && n<5)n++;
        if(!n || n>4 || (i<2 ? p[n]!='/' : p[n]!=0))return 0;
        memcpy(part,p,n);part[n]=0;value=strtoul(part,&end,16);
        if(*end)return 0;
        value=value*65535u/((1u<<(4*n))-1);
        sprintf(hex+1+i*4,"%04x",value);
        p+=n+(i<2);
    }
    return XParseColor(d,cmap,hex,color);
}
