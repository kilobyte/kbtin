#include "tintin.h"


/******************************************************************/
/* compare priorities of a and b in a semi-lexicographical order: */
/* strings generally sort in ASCIIbetical order, however numbers  */
/* sort according to their numerical values.                      */
/******************************************************************/
static int prioritycmp(const char *a, const char *b)
{
    int res;

not_numeric:
    while (*a && *a==*b && !isadigit(*a))
    {
        a++;
        b++;
    }
    if (!isadigit(*a) || !isadigit(*b))
        return (*a<*b)? -1 : (*a>*b)? 1 : 0;
    while (*a=='0')
        a++;
    while (*b=='0')
        b++;
    res=0;
    while (isadigit(*a))
    {
        if (!isadigit(*b))
            return 1;
        if (*a!=*b && !res)
            res=(*a<*b)? -1 : 1;
        a++;
        b++;
    }
    if (isadigit(*b))
        return -1;
    if (res)
        return res;
    goto not_numeric;
}

/*****************************************************/
/* strcmp() that sorts '\0' later than anything else */
/*****************************************************/
static int strlongercmp(const char *a, const char *b)
{
next:
    if (!*a)
        return *b? 1 : 0;
    if (*a==*b)
    {
        a++;
        b++;
        goto next;
    }
    if (!*b || ((unsigned char)*a) < ((unsigned char)*b))
        return -1;
    return 1;
}

bool Cstrlongercmp::operator() (const char *a, const char *b) const
{
    return strlongercmp(a, b) < 0;
}

bool Prioritycmp::operator() (const Strpair& a, const Strpair& b) const
{
    int r = prioritycmp(a.first.c_str(), b.first.c_str());
    if (r)
        return r < 0;
    return strlongercmp(a.second.c_str(), b.second.c_str()) < 0;
}
