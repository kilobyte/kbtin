/*********************************************************************/
/* file: variables.c - functions related the the variables           */
/*                             TINTIN ++                             */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include <math.h>
#include "protos/action.h"
#include "protos/alias.h"
#include "protos/chinese.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/ivars.h"
#include "protos/math.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/path.h"
#include "protos/session.h"
#include "protos/ticks.h"
#include "protos/unicode.h"
#include "protos/utils.h"


extern struct session *if_command(const char *arg, struct session *ses);

void set_variable(const char *left, const char *right, struct session *ses)
{
    set_hash(ses->myvars, left, right);
    varnum++;       /* we don't care for exactness of this */
    if (ses->mesvar[MSG_VARIABLE])
        tintin_printf(ses, "#Ok. $%s is now set to {%s}.", left, right);
}

static int builtin_var(const char *varname, char *value, struct session *ses)
{
    if (!strcmp(varname, "secstotick"))
        sprintf(value, "%lld", timetilltick(ses)/NANO);
    else if (!strcmp(varname, "TIMETOTICK"))
    {
        timens_t tt = timetilltick(ses);
        sprintf(value, "%lld.%09ld", tt/NANO, labs(tt%NANO));
    }
    else if (!strcmp(varname, "LINES"))
        sprintf(value, "%d", LINES);
    else if (!strcmp(varname, "COLS"))
        sprintf(value, "%d", COLS);
    else if (!strcmp(varname, "PATH"))
        path2var(value, ses);
    else if (!strcmp(varname, "IDLETIME"))
        nsecstr(value, current_time()-ses->idle_since);
    else if (!strcmp(varname, "SERVERIDLE"))
        nsecstr(value, current_time()-ses->server_idle_since);
    else if (!strcmp(varname, "USERIDLE"))
        nsecstr(value, current_time()-idle_since);
    else if (!strcmp(varname, "LINENUM"))
        sprintf(value, "%llu", ses->linenum);
    else if (_ && (!strcmp(varname, "LINE")
                   || !strcmp(varname, "_")))
        strcpy(value, _);
    else if (!strcmp(varname, "SESSION"))
        strcpy(value, ses->name);
    else if (!strcmp(varname, "SESSIONS"))
        seslist(value);
    else if (!strcmp(varname, "ASESSION"))
        strcpy(value, activesession->name);
    else if (!strcmp(varname, "LOGFILE"))
        strcpy(value, ses->logfile?ses->logname:"");
    else if (!strcmp(varname, "RANDOM") || !strcmp(varname, "_random"))
        sprintf(value, "%d", rand());
    else if (!strcmp(varname, "_time"))
    {
        timens_t age = current_time() - start_time;
        sprintf(value, "%lld", age/NANO);
    }
    else if (!strcmp(varname, "STARTTIME"))
        sprintf(value, "%lld.%09lld", start_time/NANO, start_time%NANO);
    else if (!strcmp(varname, "TIME"))
    {
        timens_t ct = current_time();
        sprintf(value, "%lld.%09lld", ct/NANO, ct%NANO);
    }
    else if (!strcmp(varname, "_clock"))
        sprintf(value, "%ld", (long int)time(0));
    else if (!strcmp(varname, "_msec"))
    {
        timens_t age = current_time() - start_time;
        sprintf(value, "%lld", age / (NANO/1000));
    }
    else if (!strcmp(varname, "HOME"))
    {
        const char *v=getenv("HOME");
        if (v)
            snprintf(value, BUFFER_SIZE, "%s", v);
        else
            *value=0;
    }
    else
        return 0;
    return 1;
}

/*************************************************************************/
/* copy the arg text into the result-space, but substitute the variables */
/* $<string> with the values they stand for                              */
/*                                                                       */
/* Notes by Sverre Normann (sverreno@stud.ntnu.no):                      */
/* xxxxxxxxx 1996: added 'simulation' of secstotick variable             */
/* Tue Apr 8 1997: added ${<string>} format to allow use of non-alpha    */
/*                 characters in variable name                           */
/*************************************************************************/
/* Note: $secstotick will return number of seconds to tick (from tt's    */
/*       internal counter) provided no variable named 'secstotick'       */
/*       already exists                                                  */
/*************************************************************************/
void substitute_myvars(const char *arg, char *result, struct session *ses)
{
    char varname[BUFFER_SIZE], value[BUFFER_SIZE], *v;
    int nest = 0, counter, varlen, valuelen;
    bool specvar;
    int len=strlen(arg);

    while (*arg)
    {
        if (*arg == '$')
        {                           /* substitute variable */
            counter = 0;
            while (*(arg + counter) == '$')
                counter++;
            varlen = 0;

            /* ${name} code added by Sverre Normann */
            if (*(arg+counter) != BRACE_OPEN)
            {
                /* ordinary variable which name contains no spaces and special characters  */
                specvar = false;
                while (isalpha(*(arg+varlen+counter))||
                       (*(arg+varlen+counter)=='_')||
                       isadigit(*(arg+varlen+counter)))
                    varlen++;
                if (varlen>0)
                    memcpy(varname, arg+counter, varlen);
                *(varname + varlen) = '\0';
            }
            else
            {
                /* variable with name containing non-alpha characters e.g ${a b} */
                specvar = true;
                get_arg_in_braces(arg + counter, varname, 0);
                varlen=strlen(varname);
                substitute_vars(varname, value);
                substitute_myvars(value, varname, ses);      /* RECURSIVE CALL */
            }

            if (specvar) varlen += 2; /* 2*DELIMITERS e.g. {} */

            if (counter == nest + 1)
            {
                if ((v=get_hash(ses->myvars, varname)))
                {
                    valuelen = strlen(v);
                    if ((len+=valuelen-counter-varlen) > BUFFER_SIZE-10)
                    {
                        if (!aborting)
                        {
                            tintin_eprintf(ses, "#ERROR: command+variables too long while substituting $%s%s%s.",
                                specvar?"{":"", varname, specvar?"}":"");
                            aborting=true;
                        }
                        len-=valuelen-counter-varlen;
                        goto novar;
                    }
                    strcpy(result, v);
                    result += valuelen;
                    arg += counter + varlen;
                }
                else
                {
                    if (!builtin_var(varname, value, ses))
                        goto novar;
                    valuelen=strlen(value);
                    if ((len+=valuelen-counter-varlen) > BUFFER_SIZE-10)
                    {
                        if (!aborting)
                        {
                            tintin_eprintf(ses, "#ERROR: command+variables too long while substituting $%s.", varname);
                            aborting=true;
                        }
                        len-=valuelen-counter-varlen;
                        goto novar;
                    }
                    strcpy(result, value);
                    result+=valuelen;
                    arg += counter + varlen;
                }
            }
            else
            {
novar:
                memcpy(result, arg, counter + varlen);
                result += varlen + counter;
                arg += varlen + counter;
            }
        }
        else if (*arg == BRACE_OPEN)
        {
            nest++;
            *result++ = *arg++;
        }
        else if (*arg == BRACE_CLOSE)
        {
            nest--;
            *result++ = *arg++;
        }
        else if (*arg == '\\' && *(arg + 1) == '$' && nest == 0)
        {
            arg++;
            *result++ = *arg++;
            len--;
        }
        else
            *result++ = *arg++;
    }

    *result = '\0';
}


/*************************/
/* the #variable command */
/*************************/
void variable_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    bool r=*space_out(arg);
    arg = get_arg(arg, right, 1, ses);
    if (*left && r)
    {
        set_variable(left, right, ses);
        return;
    }
    show_hashlist(ses, ses->myvars, left,
        "#THESE VARIABLES HAVE BEEN SET:",
        "#THAT VARIABLE IS NOT DEFINED.");
}

/**********************/
/* the #unvar command */
/**********************/
void unvariable_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    do
    {
        arg = get_arg(arg, left, 0, ses);
        delete_hashlist(ses, ses->myvars, left,
            ses->mesvar[MSG_VARIABLE]? "#Ok. $%s is no longer a variable." : 0,
            ses->mesvar[MSG_VARIABLE]? "#THAT VARIABLE (%s) IS NOT DEFINED." : 0);
    } while (*arg);
}


/************************/
/* the #tolower command */
/************************/
void tolower_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    WC txt[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #tolower <var> <text>");

    TO_WC(txt, right);
    for (WC *p = txt; *p; p++)
        *p = towlower(*p);
    WRAP_WC(right, txt);
    set_variable(left, right, ses);
}

/************************/
/* the #toupper command */
/************************/
void toupper_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    WC txt[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #toupper <var> <text>");

    TO_WC(txt, right);
    for (WC *p = txt; *p; p++)
        *p = towupper(*p);
    WRAP_WC(right, txt);
    set_variable(left, right, ses);
}

/***************************/
/* the #firstupper command */
/***************************/
void firstupper_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    WC txt[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #firstupper <var> <text>");

    TO_WC(txt, right);
    *txt=towupper(*txt);
    WRAP_WC(right, txt);
    set_variable(left, right, ses);
}

/***************************/
/* the #firstlower command */
/***************************/
void firstlower_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    WC txt[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #firstlower <var> <text>");

    TO_WC(txt, right);
    *txt=towlower(*txt);
    WRAP_WC(right, txt);
    set_variable(left, right, ses);
}

/***********************/
/* the #strlen command */
/***********************/
void strlen_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #strlen <var> <text>");

    sprintf(right, "%d", FLATlen(right));
    set_variable(left, right, ses);
}

/**********************/
/* the #strlen inline */
/**********************/
int strlen_inline(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg(arg, left, 1, ses);
    return FLATlen(left);
}

/****************************************************/
/* the #reverse command * By Sverre Normann         */
/* with little changes by Jakub Narebski            */
/****************************************************/
/* Syntax: #reverse {destination variable} {string} */
/****************************************************/
/* Will return the reverse of a string.             */
/****************************************************/
/* can be used to get return path using #savepath   */
/****************************************************/

/* FUNCTION:  reverse - reverses string    */
/* ARGUMENTS: src - string to be reversed  */
/*            dest - destination string    */
/* RESULT:    reversed string              */
/* NOTE:      no check                     */
static WC* revstr(WC *dest, WC *src)
{
    int ilast = WClen(src) - 1;

    for (int i = ilast; i >= 0; i--)
        dest[ilast - i] = src[i];
    dest[ilast + 1] = '\0';

    return dest;
}

/************************/
/* the #reverse command */
/************************/
void reverse_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], strvar[BUFFER_SIZE];
    WC origstring[BUFFER_SIZE], revstring[BUFFER_SIZE];

    arg = get_arg(arg, destvar, 0, ses);
    arg = get_arg(arg, strvar, 1, ses);

    if (!*destvar)
        return tintin_eprintf(ses, "#Error - Syntax: #reverse {destination variable} {string}");

    TO_WC(origstring, strvar);
    revstr(revstring, origstring);
    WRAP_WC(strvar, revstring);
    set_variable(destvar, strvar, ses);
}


/***********************/
/* the #random command */
/***********************/
void random_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], dummy;
    int low, high, number;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left || !*right)
        return tintin_eprintf(ses, "#Syntax: #random <var> <low,high>");
    if (sscanf(right, "%d, %d%c", &low, &high, &dummy) != 2)
        return tintin_eprintf(ses, "#Wrong number of range arguments in #random: got {%s}.", right);
    if (low < 0 || high < 0)
        return tintin_eprintf(ses, "#Both arguments of range in #random should be ≥0, got %d,%d.", low, high);

    if (low > high)
    {
        number = low;
        low = high, high = number;
    }
    number = low + rand() % (high - low + 1);
    sprintf(right, "%d", number);
    set_variable(left, right, ses);
}

/**********************/
/* the #random inline */
/**********************/
int random_inline(const char *arg, struct session *ses)
{
    char right[BUFFER_SIZE];
    int low, high, tmp;

    arg = get_arg(arg, right, 1, ses);
    if (!*right)
        tintin_eprintf(ses, "#Syntax: #random <low,high>");
    else if (sscanf(right, "%d, %d", &low, &high) != 2)
        tintin_eprintf(ses, "#Wrong number of range arguments in #random: got {%s}.", right);
    else if (low < 0 || high < 0)
        tintin_eprintf(ses, "#Both arguments of range in #random should be ≥0, got %d,%d.", low, high);
    else
    {
        if (low > high)
        {
            tmp = low;
            low = high, high = tmp;
        }
        return low + rand() % (high - low + 1);
    }
    return 0;
}

/************************************************************************************/
/* cut a string to width len, putting the cut point into *rstr and return the width */
/************************************************************************************/
static int cutws(WC *str, int len, WC **rstr)
{
    int w, s;

    s=0;
    while (*str)
    {
        w=wcwidth(*str);
        if (w<0)
            w=0;
        if (w && s+w>len)
            break;
        str++;
        s+=w;
    }
    *rstr=str;
    return s;
}

/*****************************************************/
/* Syntax: #postpad {dest var} {length} {text}       */
/*****************************************************/
/* Truncates text if it's too long for the specified */
/* length. Pads with spaces at the end if the text   */
/* isn't long enough.                                */
/*****************************************************/
void postpad_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], lengthstr[BUFFER_SIZE], astr[BUFFER_SIZE], *aptr;
    WC bstr[BUFFER_SIZE], *bptr;
    int len, length;

    arg = get_arg(arg, destvar, 0, ses);
    arg = get_arg(arg, lengthstr, 0, ses);
    arg = get_arg(arg, astr, 1, ses);

    if (!*lengthstr)
        return tintin_eprintf(ses, "#Error - Syntax: #postpad {dest var} {length} {text}");

    if (!sscanf(lengthstr, "%d", &length) || (length < 1) || (length > BUFFER_SIZE-10))
        return tintin_eprintf(ses, "#Error in #postpad - length has to be a positive number >0, got {%s}.", lengthstr);

    TO_WC(bstr, astr);
    len=cutws(bstr, length, &bptr);
    aptr=astr+wc_to_utf8(astr, bstr, bptr-bstr, BUFFER_SIZE-3);
    while (len<length)
        len++, *aptr++=' ';
    *aptr=0;
    set_variable(destvar, astr, ses);
}

/*****************************************************/
/* the #prepad command * By Sverre Normann           */
/* with little changes by Jakub Narebski             */
/*****************************************************/
/* Syntax: #prepad {dest var} {length} {text}        */
/*****************************************************/
/* Truncates text if it's too long for the specified */
/* length. Pads with spaces at the start if the text */
/* isn't long enough.                                */
/*****************************************************/
void prepad_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], astr[BUFFER_SIZE], lengthstr[BUFFER_SIZE], *aptr;
    WC bstr[BUFFER_SIZE], *bptr;
    int len, length;

    arg = get_arg(arg, destvar, 0, ses);
    arg = get_arg(arg, lengthstr, 0, ses);
    arg = get_arg(arg, astr, 1, ses);

    if (!*lengthstr)
        return tintin_eprintf(ses, "#Error - Syntax: #prepad {dest var} {length} {text}");

    if (!sscanf(lengthstr, "%d", &length) || (length < 1) || (length>BUFFER_SIZE-10))
        return tintin_eprintf(ses, "#Error in #prepad - length has to be a positive number >0, got {%s}.", lengthstr);

    TO_WC(bstr, astr);
    len=cutws(bstr, length, &bptr);
    aptr=astr;
    while (len<length)
        len++, *aptr++=' ';
    aptr+=wc_to_utf8(aptr, bstr, bptr-bstr, BUFFER_SIZE-3);
    *aptr=0;
    set_variable(destvar, astr, ses);
}

#define INVALID_TIME (int)0x80000000

/************************************************************/
/* parse time, return # of seconds or INVALID_TIME on error */
/************************************************************/
static int time2secs(const char *tt, struct session *ses)
{
    int w, t=0;

    if (!*tt)
    {
bad:
        tintin_eprintf(ses, "#time format should be: <#y[ears][, ] #w #d #h #m [and] #[s]> or just <#> of seconds.");
        tintin_eprintf(ses, "#got: {%s}.", tt);
        return INVALID_TIME;
    }
    t=0;
    for (;;)
    {
        char *err;
        w=strtol(tt, &err, 10);
        if (tt==err)
            goto bad;
        tt=err;
        while (*tt==' ')
            tt++;
        switch (toalower(*tt))
        {
        case 'w':
            w*=7;
            goto day;
        case 'y':
            w*=365;
        day:
        case 'd':
            w*=24;
        case 'h':
            w*=60;
        case 'm':
            w*=60;
        case 's':
            while (isalpha(*tt))
                tt++;
        case 0:
            t+=w;
            break;
        default:
            goto bad;
        }
        if (*tt==',')
            tt++;
        while (*tt==' ')
            tt++;
        if (!strncmp(tt, "and", 3))
            tt+=3;
        if (!*tt)
            break;
    }
    return t;
}

/**********************/
/* the #ctime command */
/**********************/
void ctime_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], arg2[BUFFER_SIZE], *ct, *p;
    time_t tt;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, arg2, 1, ses);
    if (!*arg2)
        tt=time(0);
    else if ((tt=time2secs(arg2, ses))==INVALID_TIME)
        return;
    p = ct = ctime_r(&tt, arg2);
    while (p && *p)
    {
        if (*p == '\n')
        {
            *p = '\0';
            break;
        }
        p++;
    }
    if (!*left)
        tintin_printf(ses, "#%s.", ct);
    else
        set_variable(left, ct, ses);
}

/*********************/
/* the #time command */
/*********************/
void time_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], ct[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, ct, 1, ses);
    if (*ct)
    {
        int t=time2secs(ct, ses);
        if (t==INVALID_TIME)
            return;
        sprintf(ct, "%d", t);
    }
    else
        sprintf(ct, "%d", (int)time(0));
    if (!*left)
        tintin_printf(ses, "#%s.", ct);
    else
        set_variable(left, ct, ses);
}

/**************************/
/* the #localtime command */
/**************************/
void localtime_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], ct[BUFFER_SIZE];
    time_t t;
    struct tm ts;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, ct, 1, ses);
    if (*ct)
    {
        t=time2secs(ct, ses);
        if (t==INVALID_TIME)
            return;
        sprintf(ct, "%ld", (long)t);
    }
    else
        t=time(0);
    localtime_r(&t, &ts);
    sprintf(ct, "%02d %02d %02d  %02d %02d %04d  %d %d %d",
                    ts.tm_sec, ts.tm_min, ts.tm_hour,
                    ts.tm_mday, ts.tm_mon+1, ts.tm_year+1900,
                    ts.tm_wday, ts.tm_yday+1, ts.tm_isdst);
    if (!*left)
        tintin_printf(ses, "#%s.", ct);
    else
        set_variable(left, ct, ses);
}

/***********************/
/* the #gmtime command */
/***********************/
void gmtime_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], ct[BUFFER_SIZE];
    time_t t;
    struct tm ts;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, ct, 1, ses);
    if (*ct)
    {
        t=time2secs(ct, ses);
        if (t==INVALID_TIME)
            return;
        sprintf(ct, "%ld", (long)t);
    }
    else
        t=time(0);
    gmtime_r(&t, &ts);
    sprintf(ct, "%02d %02d %02d  %02d %02d %04d  %d %d %d",
                    ts.tm_sec, ts.tm_min, ts.tm_hour,
                    ts.tm_mday, ts.tm_mon+1, ts.tm_year+1900,
                    ts.tm_wday, ts.tm_yday+1, ts.tm_isdst);
    if (!*left)
        tintin_printf(ses, "#%s.", ct);
    else
        set_variable(left, ct, ses);
}


/**************************/
/* the #substring command */
/**************************/
void substring_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], mid[BUFFER_SIZE], right[BUFFER_SIZE], *p;
    WC buf[BUFFER_SIZE], *lptr, *rptr;
    int l, r, s, w;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, mid, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    l=strtol(mid, &p, 10);
    if (*p==',')
        r=strtol(p+1, &p, 10);
    else
        r=l;
    if (l<1)
        l=1;

    if (!*left || (p==mid) || (*p) || (r<0))
        return tintin_eprintf(ses, "#SYNTAX: substr <var> <l>[,<r>] <string>");

    p=mid;
    TO_WC(buf, right);
    lptr=buf;
    s=1;
    while (*lptr)
    {
        w=wcwidth(*lptr);
        if (w<0)
            w=0;
        if (w && s>=l)
            break;  /* skip incomplete CJK chars with all modifiers */
        lptr++;
        s+=w;
    }
    if (s>l)
        *p++=' ';   /* the left edge is cut in half */
    rptr=lptr;
    while (w=wcwidth(*rptr), *rptr)
    {
        if (w<0)
            w=0;
        if (w && s+w>r+1)
            break;  /* skip incomplete CJK chars with all modifiers */
        rptr++;
        s+=w;
    }
    if (rptr>lptr)
        p+=wc_to_utf8(p, lptr, rptr-lptr, BUFFER_SIZE-3);
    if (s==r && w==2)
        *p++=' ';   /* the right edge is cut */
    *p=0;
    set_variable(left, mid, ses);
}

/***********************/
/* the #strcmp command */
/***********************/
struct session *strcmp_command(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], cmd[BUFFER_SIZE];

    line = get_arg(line, left, 0, ses);
    line = get_arg(line, right, 0, ses);
    line = get_arg_in_braces(line, cmd, 1);

    if (!*cmd)
    {
        tintin_eprintf(ses, "#Syntax: #strcmp <a> <b> <command> [#else <command>]");
        return ses;
    }

    if (!strcmp(left, right))
        return parse_input(cmd, true, ses);

    line = get_arg_in_braces(line, left, 0);
    if (*left == tintin_char)
    {
        if (is_abrev(left + 1, "else"))
        {
            line = get_arg_in_braces(line, right, 1);
            ses=parse_input(right, true, ses);
        }
        if (is_abrev(left + 1, "elif"))
            ses=if_command(line, ses);
    }
    return ses;
}

/**********************/
/* the #strcmp inline */
/**********************/
int strcmp_inline(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    line = get_arg(line, left, 0, ses);
    line = get_arg(line, right, 1, ses);

    return !strcmp(left, right);
}

/***************************************/
/* the #ifstrequal command             */
/***************************************/
/* (mainstream tintin++ compatibility) */
/***************************************/
struct session *ifstrequal_command(const char *line, struct session *ses)
{
    return strcmp_command(line, ses);
}

/*************************/
/* the #ifexists command */
/*************************/
struct session *ifexists_command(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], cmd[BUFFER_SIZE];

    line = get_arg(line, left, 0, ses);
    line = get_arg_in_braces(line, cmd, 1);

    if (!*cmd)
    {
        tintin_eprintf(ses, "#Syntax: #ifexists <varname> <command> [#else <command>]");
        return ses;
    }

    if (get_hash(ses->myvars, left))
        return parse_input(cmd, true, ses);

    line = get_arg_in_braces(line, left, 0);
    if (*left == tintin_char)
    {
        if (is_abrev(left + 1, "else"))
        {
            line = get_arg_in_braces(line, cmd, 1);
            ses=parse_input(cmd, true, ses);
        }
        if (is_abrev(left + 1, "elif"))
            ses=if_command(line, ses);
    }
    return ses;
}

/*********************/
/* the #ctoi command */
/*********************/
void ctoi_command(const char* arg, struct session* ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg=get_arg(arg, left, 0, ses);
    arg=get_arg(arg, right, 1, ses);

    if (!*left || !*right)
        return tintin_eprintf(ses, "#Syntax: #ctoi <var> <text>");

    ctoi(right);
    set_variable(left, right, ses);
}

/*****************************/
/* the #initvariable command */
/*****************************/
void initvariable_command(const char* arg, struct session* ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg=get_arg(arg, left, 0, ses);
    arg=get_arg(arg, right, 1, ses);

    if (!*left)
        tintin_eprintf(ses, "#Syntax: #initvar <var> <value>");
    else if (!get_hash(ses->myvars, left))
        set_variable(left, right, ses);
}

/*********************/
/* the #angle inline */
/*********************/
num_t angle_inline(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    line = get_arg(line, left, 0, ses);
    line = get_arg(line, right, 1, ses);
    if (!*left || !*right)
    {
        tintin_eprintf(ses, "#Error: #angle requires two args.");
        return 0;
    }

    num_t x = eval_expression(left, ses);
    num_t y = eval_expression(right, ses);

    num_t a = atan2(y, x) * 180 * M_1_PI * DENOM;
    if (a < 0)
        a += 360*DENOM;
    return a;
}

/*******************/
/* the #sin inline */
/*******************/
num_t sinus_inline(const char *line, struct session *ses)
{
    char arg[BUFFER_SIZE];

    line = get_arg(line, arg, 1, ses);
    if (!*arg)
    {
        tintin_eprintf(ses, "#Error: #sin require an argument.");
        return 0;
    }

    num_t x = eval_expression(arg, ses);
    return sin(x * M_PI / 180 / DENOM) * DENOM;
}

/*******************/
/* the #cos inline */
/*******************/
num_t cosinus_inline(const char *line, struct session *ses)
{
    char arg[BUFFER_SIZE];

    line = get_arg(line, arg, 1, ses);
    if (!*arg)
    {
        tintin_eprintf(ses, "#Error: #cos require an argument.");
        return 0;
    }

    num_t x = eval_expression(arg, ses);
    return cos(x * M_PI / 180 / DENOM) * DENOM;
}
