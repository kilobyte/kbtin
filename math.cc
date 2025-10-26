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
#ifdef __SIZEOF_INT128__
    __uint128_t rem = ((__uint128_t)(llabs(x)%llabs(y)))*DENOM;
    num_t zl = rem/llabs(y) * (((x^y)>=0)?+1:-1);
#else
    // 128-bit floats have enough precision, and I don't care enough
    // about 32-bit archs to optimize.  There's enough overhead
    // elsewhere that math calculations are non-critical, anyway.
    long double rem = ((long double)(llabs(x)%llabs(y)))*DENOM;
    num_t zl = roundl(rem/llabs(y)) * (((x^y)>=0)?+1:-1);
#endif
    return zh*DENOM + zl;
}

int num2str(char *buf, num_t v)
{
    char *b = buf + sprintf(buf, "%s%" PRId64, (v<0 && v>-DENOM)? "-":"",
        (int64_t)(v/DENOM));
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
    return sprintf(buf, "%s%lld.%06d", (v<0 && v>-NANO)? "-":"",
        v/NANO, abs((int)(v%NANO/1000)));
}

int nsecstr(char *buf, timens_t v)
{
    if (!(v % NANO))
        return sprintf(buf, "%lld", v/NANO);
    return sprintf(buf, "%s%lld.%09d", (v<0 && v>-NANO)? "-":"",
        v/NANO, abs((int)(v%NANO)));
}

num_t str2num(const char *str, char **err)
{
    num_t xh = strtol(str, err, 10) * DENOM;
    if (**err!='.')
        return xh;
    bool neg = *str=='-';
    num_t y = DENOM*1000;
    num_t x = 0;
    for (str = *err+1; isadigit(*str); str++)
    {
        x += y * (*str-'0');
        y/=10;
    }
    x = (x+4999)/10000;
    *err = (char*)str;
    return neg? xh-x : xh+x;
}

/* parse a timestamp */
timens_t str2timens(const char *str, char **err)
{
    timens_t t = strtol(str, err, 10) * NANO;
    if (**err!='.')
        return t;
    num_t x = (*str=='-')? -NANO:NANO;
    for (str = *err+1; isadigit(*str); str++)
        t += (*str-'0') * (x/= 10);
    *err = (char*)str;
    return t;
}
