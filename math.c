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

static void ass(num_t x, num_t y, const char *what)
{
    if (x==y)
        return;
    fprintf(stderr, "Assert failed: %s is %"PRId64", expected %"PRId64"\n", what, x, y);
}

#define ASS(x,y) ass((x), (y), #x)
#define ASS4(op,x,y,e) do {\
    ASS(op(DENOM*x,DENOM*y),DENOM*e);\
    ASS(op(DENOM*x,DENOM*-y),-DENOM*e);\
    ASS(op(DENOM*-x,DENOM*y),-DENOM*e);\
    ASS(op(DENOM*-x,DENOM*-y),DENOM*e);} while(0)

void asserts_math(void)
{
    ASS4(nmul, 2, 2, 4);
    ASS4(nmul, 10, 1/10, 1);
    ASS4(ndiv, 2, 2, 1);
    ASS4(ndiv, 2, 1, 2);
    ASS4(ndiv, 10, 3, 10/3);
}
