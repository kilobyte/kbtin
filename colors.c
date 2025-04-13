#include "tintin.h"
#include "protos/globals.h"


const int rgbbgr[8]={0,4,2,6,1,5,3,7};

static int getco(const char *restrict txt, const char **err)
{
    if (!isadigit(txt[1]))
    {
        *err=txt+1;
        return txt[0]-'0';
    }

    if (!isadigit(txt[2]))
    {
        *err=txt+2;
        int c = (txt[0]-'0')*10+txt[1]-'0';
        if (c >= 16)
            return c - 16 + 232; /* 24-level color ramp */
        return c;
    }

    if (!isadigit(txt[3]))
    {
        if (txt[0]<'6' && txt[1]<'6' && txt[2]<'6')
        {
            *err=txt+3;
            return 16 + 36*(txt[0]-'0') + 6*(txt[1]-'0') + (txt[2]-'0');
        }
    }

    return -1U>>1;
}

int getcolor(const char *restrict*restrict ptr, int *restrict color, bool allow_minus_token)
{
    unsigned fg, bg, blink;
    const char *txt=*ptr;

    if (*(txt++)!='~')
        return 0;
    if (allow_minus_token&&(*txt=='-')&&(*(txt+1)=='1')&&(*(txt+2)=='~'))
    {
        *color=-1;
        *ptr+=3;
        return 1;
    }
    if (isadigit(*txt))
    {
        const char *err;
        fg=getco(txt, &err);
        if (fg > CFG_MASK)
            return 0;
        txt=err;
    }
    else if (*txt==':')
        fg=(*color==-1)? 7 : ((*color)&CFG_MASK);
    else
        return 0;
    if (*txt=='~')
    {
        *color=fg;
        *ptr=txt;
        return 1;
    }
    if (*txt!=':')
        return 0;
    if (isadigit(*++txt))
    {
        const char *err;
        bg=getco(txt, &err);
        if (bg > CBG_MAX)
            return 0;
        txt=err;
    }
    else
        bg=(*color==-1)? 0 : ((*color&CBG_MASK)>>CBG_AT);
    if (*txt=='~')
    {
        *color=bg << CFG_BITS | fg;
        *ptr=txt;
        return 1;
    }
    if (*txt!=':')
        return 0;
    if (isadigit(*++txt))
    {
        char *err;
        blink=strtol(txt, &err, 10);
        if (blink > CFL_MAX)
            return 0;
        txt=err;
    }
    else
        blink=(*color==-1)? 0 : (*color >> CFL_AT);
    if (*txt!='~')
        return 0;
    *color=blink << CFL_AT | bg << CBG_AT | fg;
    *ptr=txt;
    return 1;
}

static int setco(char *txt, int c)
{
    if (c<10)
        return txt[0]=c+'0', 1;
    if (c<16)
        return txt[0]='1', txt[1]=c+'0'-10, 2;
    if (c>=232)
        return sprintf(txt, "%d", c+16-232);
    c-=16;
    txt[0]='0'+c/36;
    txt[1]='0'+(c/6)%6;
    txt[2]='0'+c%6;
    return 3;
}

int setcolor(char *txt, int c)
{
    if (c==-1)
        return sprintf(txt, "~-1~");
    char *txt0 = txt;
    *txt++='~';
    txt+=setco(txt, c&CFG_MASK);
    if (c < 1<<CBG_AT)
        return *txt++='~', *txt=0, txt-txt0;
    *txt++=':';
    txt+=setco(txt, (c&CBG_MASK)>>CBG_AT);
    if (c < 1<<CFL_AT)
        return *txt++='~', *txt=0, txt-txt0;
    return txt-txt0+sprintf(txt, ":%d~", c>>CFL_AT);
}

char* ansicolor(char *s, int c)
{
    *s++=27, *s++='[', *s++='0';
    int k = c & CFG_MASK;
    if (k < 16)
    {
        if (!(k&8))
            *s++=';', *s++='3';
        else if (bold)
            *s++=';', *s++='1', *s++=';', *s++='3';
        else
            *s++=';', *s++='9';
        *s++='0'+rgbbgr[k&7];
    }
    else
        s+=sprintf(s, ";38;5;%d", k);
    k = (c & CBG_MASK) >> CBG_AT;
    if (k < 16)
    {
        if (k&8)
            *s++=';', *s++='1', *s++='0';
        else
            *s++=';', *s++='4';
        *s++='0'+rgbbgr[k&7];
    }
    else
        s+=sprintf(s, ";48;5;%d", k);
    if (c>>=CFL_AT)
    {
        if (c&C_BLINK)
            *s++=';', *s++='5';
        if (c&C_ITALIC)
            *s++=';', *s++='3';
        if (c&C_UNDERLINE)
            *s++=';', *s++='4';
        if (c&C_STRIKETHRU)
            *s++=';', *s++='9';
    }
    *s++='m';
    return s;
}
