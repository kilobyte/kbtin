/*********************************************************************/
/* file: variables.c - functions related the the variables           */
/*                             TINTIN ++                             */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include <math.h>
#include <time.h>
#include "protos/action.h"
#include "protos/alias.h"
#include "protos/chinese.h"
#include "protos/colors.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/eval.h"
#include "protos/math.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/path.h"
#include "protos/session.h"
#include "protos/ticks.h"
#include "protos/unicode.h"
#include "protos/utils.h"
#include "protos/vars.h"


extern struct session *if_command(const char *arg, struct session *ses);
static int cutws(WC *str, int len, WC **rstr, int *color);

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

    sprintf(right, "%d", utf8_width(right));
    set_variable(left, right, ses);
}

/**********************/
/* the #strlen inline */
/**********************/
int strlen_inline(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg(arg, left, 1, ses);
    return utf8_width(left);
}

static int cwidth(char *s)
{
    WC txt[BUFFER_SIZE], *dum;
    int color=7;

    utf8_to_wc(txt, s, BUFFER_SIZE-1);
    return cutws(txt, BUFFER_SIZE-1, &dum, &color);
}

/*************************/
/* the #strwidth command */
/*************************/
void strwidth_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #strwidth <var> <text>");

    sprintf(right, "%d", cwidth(right));
    set_variable(left, right, ses);
}

/************************/
/* the #strwidth inline */
/************************/
int strwidth_inline(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg(arg, left, 1, ses);
    return cwidth(left);
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
static int cutws(WC *str, int len, WC **rstr, int *color)
{
    char cbuf[BUFFER_SIZE], *cp=cbuf;
    for (WC *sp=str; *sp; sp++)
        *cp++ = (*sp>'~')? 'x' : *sp; // only color codes count
    cp=cbuf;

    int s=0;
    while (*str)
    {
        if (*str=='~')
        {
            const char *cend = cp;
            if (getcolor(&cend, color, true))
            {
                str+=(cend-cp)+1;
                cp+=(cend-cp)+1;
                continue;
            }
        }

        int w=wcwidth(*str);
        if (w<0)
            w=0;
        if (w && s+w>len)
            break;
        str++; cp++;
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
    int len, length, dummy=-1;

    arg = get_arg(arg, destvar, 0, ses);
    arg = get_arg(arg, lengthstr, 0, ses);
    arg = get_arg(arg, astr, 1, ses);

    if (!*lengthstr)
        return tintin_eprintf(ses, "#Error - Syntax: #postpad {dest var} {length} {text}");

    if (!sscanf(lengthstr, "%d", &length) || (length < 1) || (length > BUFFER_SIZE-10))
        return tintin_eprintf(ses, "#Error in #postpad - length has to be a positive number >0, got {%s}.", lengthstr);

    TO_WC(bstr, astr);
    len=cutws(bstr, length, &bptr, &dummy);
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
    int len, length, dummy=-1;

    arg = get_arg(arg, destvar, 0, ses);
    arg = get_arg(arg, lengthstr, 0, ses);
    arg = get_arg(arg, astr, 1, ses);

    if (!*lengthstr)
        return tintin_eprintf(ses, "#Error - Syntax: #prepad {dest var} {length} {text}");

    if (!sscanf(lengthstr, "%d", &length) || (length < 1) || (length>BUFFER_SIZE-10))
        return tintin_eprintf(ses, "#Error in #prepad - length has to be a positive number >0, got {%s}.", lengthstr);

    TO_WC(bstr, astr);
    len=cutws(bstr, length, &bptr, &dummy);
    aptr=astr;
    while (len<length)
        len++, *aptr++=' ';
    aptr+=wc_to_utf8(aptr, bstr, bptr-bstr, BUFFER_SIZE-3);
    *aptr=0;
    set_variable(destvar, astr, ses);
}


/**************************/
/* the #substring command */
/**************************/
void substring_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], mid[BUFFER_SIZE], right[BUFFER_SIZE], *p;
    WC buf[BUFFER_SIZE];
    int l, r;

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
    WC *lb=buf;
    int color=-1;
    if (l>1)
    {
        int lw=cutws(buf, l-1, &lb, &color);
        if (color!=-1)
            p+=setcolor(p, color);
        if (lw==l-2 && *lb) /* 1 column short and not end of string → half of a CJK char */
        {
            *p++=' ';
            l++;
            lb++;
            while (*lb && !wcwidth(*lb)) /* eat combining chars as well */
                lb++;
        }
    }

    WC *rb;
    int rw=cutws(lb, r+1-l, &rb, &color);
    p+=wc_to_utf8(p, lb, rb-lb, mid+BUFFER_SIZE-3-p);
    if (rw==r-l && *rb) /* half of a CJK char */
        *p++=' ', *p=0;
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
/* the #trim command */
/*********************/
void trim_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], str[BUFFER_SIZE], *s, *e, *p;

    arg = get_arg(arg, destvar, 0, ses);
    arg = get_arg(arg, str, 1, ses);

    if (!*destvar)
        return tintin_eprintf(ses, "#Error - syntax: #trim {dest var} {text}");

    s = str;
    while (isaspace(*s))
        s++;
    for (p=e=s; *p; p++)
        if (!isaspace(*p))
            e = p+1;
    *e=0;
    set_variable(destvar, s, ses);
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
        while (isaspace(*tt))
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
            while (is7alpha(*tt))
                tt++;
        case 0:
            t+=w;
            break;
        default:
            goto bad;
        }
        if (*tt==',')
            tt++;
        while (isaspace(*tt))
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
        tintin_eprintf(ses, "#Error: #sin requires an argument.");
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
        tintin_eprintf(ses, "#Error: #cos requires an argument.");
        return 0;
    }

    num_t x = eval_expression(arg, ses);
    return cos(x * M_PI / 180 / DENOM) * DENOM;
}

/********************/
/* the #sqrt inline */
/********************/
num_t sqrt_inline(const char *line, struct session *ses)
{
    char arg[BUFFER_SIZE];

    line = get_arg(line, arg, 1, ses);
    if (!*arg)
    {
        tintin_eprintf(ses, "#Error: #sqrt requires an argument.");
        return 0;
    }

    num_t x = eval_expression(arg, ses);
    if (x < 0)
    {
        tintin_eprintf(ses, "#Error: you're imagining that sqrt can take arg < 0");
        return 0;
    }

    return sqrt(x * 1.0 / DENOM) * DENOM;
}

/*******************/
/* the #abs inline */
/*******************/
num_t abs_inline(const char *line, struct session *ses)
{
    char arg[BUFFER_SIZE];

    line = get_arg(line, arg, 1, ses);
    if (!*arg)
    {
        tintin_eprintf(ses, "#Error: #abs requires an argument.");
        return 0;
    }

    num_t x = eval_expression(arg, ses);
    if (x < 0)
        x = -x;
    return x;
}

/*********************/
/* the #round inline */
/*********************/
num_t round_inline(const char *line, struct session *ses)
{
    char arg[BUFFER_SIZE];

    line = get_arg(line, arg, 1, ses);
    if (!*arg)
    {
        tintin_eprintf(ses, "#Error: #round requires an argument.");
        return 0;
    }

    num_t x = eval_expression(arg, ses);
    // round away from 0
    x += (x >= 0)? DENOM/2 : -DENOM/2;
    return x / DENOM * DENOM;
}
