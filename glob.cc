/*
 * match -- returns 1 if `string' satisfised `regex' and 0 otherwise
 * stolen from Spencer Sun: only recognizes * and \ as special characters
 */

#include "tintin.h"

bool match(const char *regex, const char *string)
{
    const char *rp = regex, *sp = string, *save;
    char ch;

    while (*rp)
    {
        switch (ch = *rp++)
        {
        case '*':
            if (!*sp)           /* match empty string at end of `string' */
                return !*rp;    /* but only if we're done with the pattern */
            /* greedy algorithm: save starting location, then find end of string */
            save = sp;
            sp += strlen(sp);
            do
            {
                if (match(rp, sp))     /* return success if we can match here */
                    return 1;
            } while (--sp >= save);    /* otherwise back up and try again */
            /*
             * Backed up all the way to starting location (i.e. `*' matches
             * empty string) and we _still_ can't match here.  Give up.
             */
            return false;
        case '\\':
            if ((ch = *rp++))
            {
                /* if not end of pattern, match next char explicitly */
                if (ch != *sp++)
                    return false;
                break;
            }
            /* else FALL THROUGH to match a backslash */
        default:                       /* normal character */
            if (ch != *sp++)
                return false;
            break;
        }
    }
    /*
     * OK, we successfully matched the pattern if we got here.  Now return
     * a match if we also reached end of string, otherwise failure
     */
    return !*sp;
}


bool is_literal(const char *txt)
{
    return !strchr(txt, '*') && !strchr(txt, '\\');
}


bool find(const char *text, const char *pattern, int *from, int *to, const char *fastener)
{
    const char *txt;
    char *a, *b, *pat, m1[BUFFER_SIZE], m2[BUFFER_SIZE];
    int i;

    if (fastener)
    {
        txt=strstr(text, fastener);
        if (!txt)
            return false;
        *from=txt-text;
        if (strchr(pattern, '*'))
            *to=strlen(text);
        else
            *to=*from+strlen(fastener);
        return true;
    }

    txt=text;
    if (*pattern=='^')
    {
        for (pattern++;(*pattern)&&(*pattern!='*');)
            if (*(pattern++)!=*(txt++))
                return false;
        if (!*pattern)
        {
            *from=0;
            *to=txt-text;
            return true;
        }
        strcpy(m1, pattern);
        pat=m1;
    }
    else
    {
        const char* star = strchr(pattern, '*');
        if (!star)
        {
            const char* fixed = strstr(txt, pattern);
            if (fixed)
            {
                *from = fixed - text;
                *to = *from + strlen(pattern);
                return true;
            }
            else
                return false;
        }

        i = star - pattern;
        strcpy(m1, pattern);
        m1[i]=0;
        pat=m1;
        txt=strstr(txt, pat);
        if (!txt)
            return false;
        *from=txt-text;
        txt+=i;
        pat+=i+1;
        while (*pat=='*')
            pat++;
    }

    i=strlen(pat);
    if (!*pat)
    {
        *to=strlen(text);
        return true;
    }
    a=pat;
    b=pat+i-1;
    while (a<b)
    {
        char c=*a;
        *a++=*b;
        *b--=c;
    }
    i=strlen(txt);
    for (a=m2+i-1;*txt;)
        *a--=*txt++;
    m2[i]=0;
    txt=m2;
    *to=0;
    do
    {
        b=strchr(pat, '*');
        if (b)
            *b=0;
        a=strstr(txt, pat);
        if (!a)
            return false;
        if (!*to)
            *to = strlen(text)-(a-txt);
        int len=strlen(pat);
        txt=a+len;
        if (b)
            pat=b;
        else
            pat+=len;
    } while (*pat);
    return true;
}
