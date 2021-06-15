#include "tintin.h"
#include <inttypes.h>

num_t nmul(num_t x, num_t y)
{
    /* old compilers redundantly require an explicit message */
    _Static_assert(DENOM < 0x100000000ULL, "DENOM not 32-bit");

    num_t xh = x/DENOM;
    num_t xl = x%DENOM;
    num_t yh = y/DENOM;
    num_t yl = y%DENOM;

    return xh*yh*DENOM
        +  xh*yl
        +  xl*yh
        +  xl*yl/DENOM;
}

num_t ndiv(num_t x, num_t y)
{
    num_t zh = x/y;
    uint64_t rem = ((uint64_t)(llabs(x)%llabs(y)))*DENOM;
    num_t zl = rem/llabs(y) * (((x^y)>=0)?+1:-1);
    return zh*DENOM + zl;
}

int num2str(char *buf, num_t v)
{
    char *b = buf + sprintf(buf, "%"PRId64, (int64_t)(v/DENOM));
    unsigned x = llabs(v%DENOM);
    if (!x)
        return b-buf;
    *b++='.';
    int dig = 10;
    do
    {
        unsigned r = x/(DENOM/10);
        *b++='0'+r;
        x=(x-r*(DENOM/10))*10;
    } while (x && --dig);
    *b=0;
    return b-buf;
}

int usecstr(char *buf, timens_t v)
{
    if (!(v % NANO))
        return sprintf(buf, "%lld", v/NANO);
    int usec = v%NANO/1000;
    if (usec < 0)
        usec+=1000000;
    return sprintf(buf, "%lld.%06d", v/NANO, usec);
}

num_t str2num(const char *str, char **err)
{
    num_t xh = strtol(str, err, 10) * DENOM;
    if (**err!='.')
        return xh;
    num_t y = DENOM*1000;
    num_t x = 0;
    for (str = *err+1; isadigit(*str); str++)
    {
        x += y * (*str-'0');
        y/=10;
    }
    *err = (char*)str;
    return xh + (x+4999)/10000;
}

/* parse a timestamp */
timens_t str2timens(const char *str, char **err)
{
    timens_t t = strtol(str, err, 10) * NANO;
    if (**err!='.')
        return t;
    num_t x = NANO;
    for (str = *err+1; isadigit(*str); str++)
        t += (*str-'0') * (x/= 10);
    *err = (char*)str;
    return t;
}
