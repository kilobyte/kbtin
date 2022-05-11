#include "tintin.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/vars.h"


/*****************************************************************/
/* the #listlength command * By Sverre Normann                   */
/*****************************************************************/
/* Syntax: #listlength {destination variable} {list}             */
/*****************************************************************/
/* Note: This will return the number of items in the list.       */
/*       An item is either a word, or grouped words in brackets. */
/*       Ex:  #listl {listlength} {smile {say Hi!} flip bounce}  */
/*            -> listlength = 4                                  */
/*****************************************************************/
void listlength_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], list[BUFFER_SIZE],
    temp[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Error - Syntax: #listlength {dest var} {list}");

    get_arg(arg, list, 1, ses);
    arg = list;
    int i=0;
    do
    {
        if (*arg) i++;
        arg = get_arg_in_braces(arg, temp, 0);
    } while (*arg);
    sprintf(temp, "%d", i);
    set_variable(left, temp, ses);
}

/**************************/
/* the #listlength inline */
/**************************/
int listlength_inline(const char *arg, struct session *ses)
{
    char list[BUFFER_SIZE], temp[BUFFER_SIZE];

    arg=get_arg(arg, list, 0, ses);
    arg = list;
    int i=0;
    do {
        if (*arg) i++;
        arg = get_arg_in_braces(arg, temp, 0);
    } while (*arg);
    return i;
}


static int find_item(const char *item, const char *list)
{
    char temp[BUFFER_SIZE];

    int i=0;
    do {
        i++;
        list = get_arg_in_braces(list, temp, 0);
        if (match(item, temp))
            return i;
    } while (*list);
    return 0;
}

/*************************/
/* the #finditem command */
/*************************/
void finditem_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], list[BUFFER_SIZE], item[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, item, 0, ses);
    arg = get_arg(arg, list, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Error - Syntax: #finditem {dest var} {item} {list}");

    sprintf(item, "%d", find_item(item, list));
    set_variable(left, item, ses);
}

/************************/
/* the #finditem inline */
/************************/
int finditem_inline(const char *arg, struct session *ses)
{
    char list[BUFFER_SIZE], item[BUFFER_SIZE];

    arg = get_arg(arg, item, 0, ses);
    arg = get_arg(arg, list, 1, ses);
    return find_item(item, list);
}


/********************************************************************/
/* the #getitem command * By Sverre Normann                         */
/********************************************************************/
/* Syntax: #getitem {destination variable} {item number} {list}     */
/********************************************************************/
/* Note: This will return a specified item from a list.             */
/*       An item is either a word, or grouped words in brackets.    */
/*       FORTRAN type indices (first element has index '1' not '0') */
/*       Ex:  #geti {dothis} {2} {smile {say Hi!} flip bounce}      */
/*            -> dothis = say Hi!                                   */
/********************************************************************/
void getitem_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], itemnrtxt[BUFFER_SIZE],
    list[BUFFER_SIZE], temp1[BUFFER_SIZE];
    int itemnr;

    arg = get_arg(arg, destvar, 0, ses);
    arg = get_arg(arg, itemnrtxt, 0, ses);

    if (!*destvar || !*itemnrtxt)
        return tintin_eprintf(ses, "#Error - Syntax: #getitem {destination variable} {item number} {list}");

    if (sscanf(itemnrtxt, "%d", &itemnr) != 1)
        return tintin_eprintf(ses, "#Error in #getitem - expected a _number_ as item number, got {%s}.", itemnrtxt);

    if (itemnr<=0)
        return tintin_eprintf(ses, "#Error getitem: index must be >0, got %d", itemnr);

    get_arg(arg, list, 1, ses);
    arg = list;
    int i=0;
    do {
        arg = get_arg_in_braces(arg, temp1, 0);
        i++;
    } while (i!=itemnr);

    if (*temp1)
        set_variable(destvar, temp1, ses);
    else
    {
        set_variable(destvar, "", ses);
        if (ses->mesvar[MSG_VARIABLE])
            tintin_printf(ses, "#Item doesn't exist!");
    }
}

/*************************************************************/
/* the #isatom command * By Jakub Narebski                   */
/*************************************************************/
/* Syntax: #isatom {variable} {expression}                   */
/*************************************************************/
/*  Note: this will set variable to 0 if expression is list  */
/*        and 1 if it contains single token = atom           */
/*        i.e. it checks if expression is simple (is atom)   */
/* NOTE1: 'atom' is atom but '{atom}' is list                */
/* NOTE2: all variables substitutions in expression are done */
/*************************************************************/
/*    Ex: #isatom {log} {atom}   -> log = 1                  */
/*        #isatom {log} {{atom}} -> log = 0                  */
/*        #isatom {log} {a list} -> log = 0                  */
/*                                                           */
/* To be substituted by appropriate #if expression           */
/*************************************************************/

/* First we have function which does necessary stuff */
/* Argument: after all substitutions, with unnecessary surrounding */
/*           spaces removed (e.g. ' {atom}' is _not_ an atom */
bool isatom(const char *arg)
{
    int last = strlen(arg);
    if ((arg[0]    == BRACE_OPEN) &&
            (arg[last] == BRACE_CLOSE))
        /* one element list = '{elem}' */
        return false;

    return !strchr(arg, ' ');
        /* argument contains spaces i.e. = 'elem1 elem2' */
        /* this is incompatibile with supposed " behaviour */
}

/***********************/
/* the #isatom command */
/***********************/
void isatom_command(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], temp[8];

    line = get_arg(line, left, 0, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #isatom <dest. var> <list>");

    line = get_arg(line, right, 1, ses);
    sprintf(temp, "%d", isatom(right));
    set_variable(left, temp, ses);
}


/**********************/
/* the #isatom inline */
/**********************/
int isatom_inline(const char *arg, struct session *ses)
{
    char list[BUFFER_SIZE];

    arg = get_arg(arg, list, 1, ses);
    return isatom(list);
}


/*******************************************************************/
/* the #splitlist command * By Jakub Narebski                      */
/*******************************************************************/
/* Syntax: #splitlist {head} {tail} {list} [{head length}]         */
/*******************************************************************/
/*  Note: <head> and <tail> are destination variables,             */
/*        <list> is expression which is list (possibly             */
/*        containing only atom eg. <list>='1').                    */
/*        This will set <head> to head of the list i.e. it will    */
/*        contain elements 1..<head length> from list;             */
/*        <tail> will be set to rest of the list i.e. it will      */
/*        contain elements from element number <head length>+1     */
/*        to the last element of the list.                         */
/*        Both <head> and <tail> can be empty after command.       */
/*        DEFAULT <head length> is 1.                              */
/*******************************************************************/
/*    Ex: #splitlist {head} {tail} {smile {say Hi!} flip bounce}   */
/*        -> head = smile                                          */
/*        -> tail = {say Hi!} bounce                               */
/*    Ex: #splitlist {head} {tail} {{smile wild} Hi all}           */
/*        -> head = smile wild                                     */
/*        -> tail = Hi all                                         */
/*    Ex: #splitlist {head} {tail} {smile {say Hi} {bounce a}} {2} */
/*        -> head = smile {say Hi}                                 */
/*        -> tail = bounce a                                       */
/*        CHANGE: now it's tail-> {bounce a} to allow iterators    */
/*******************************************************************/

/* FUNCTION:  get_split_pos - where to split                 */
/* ARGUMENTS: list - list (after all substitutions done)     */
/*            head_length - where to split                   */
/* RESULT:    pointer to elements head_length+1...           */
/*            or pointer to '\0' or empty string ""          */
/*            i.e. to character after element No head_length */
static char* get_split_pos(char *list, int head_length)
{
    int i = 0;
    char temp[BUFFER_SIZE];

    if (!list[0]) /* empty string */
        return list;

    if (head_length > 0)
    { /* modified #getitemnr code */
        do {
            list = (char*)get_arg_in_braces(list, temp, 0);
            i++;
        } while (i != head_length);

        return list;
    }
    else /* head_length == 0 */
        return list;
}


/* FUNCTION:  is_braced_atom - checks if list is braced atom          */
/* ARGUMENTS: beg - points to the first character of list             */
/*            end - points to the element after last (usually '\0')   */
/*            ses - session; used only for error handling             */
/* RESULT:    true if list is braced atom e.g. '{atom}'               */
/*            i.e. whole list begins with BRACE_OPEN end ends with    */
/*            BRACE_CLOSE and whole is inside group (inside braces)   */
static bool is_braced_atom_2(const char *beg, const char *end, struct session *ses)
{
    /* we define where list ends */
#define AT_END(beg, end) (!*beg || beg >= end)
#define NOT_AT_END(beg, end) (*beg && beg < end)

    int nest = 0;

    if (AT_END(beg, end)) /* string is empty */
        return false;

    if (*beg!=BRACE_OPEN)
        return false;

    while (NOT_AT_END(beg, end) && !(*beg == BRACE_CLOSE && nest == 0))
    {
        if (*beg=='\\') /* next element is taken verbatim i.e. as is */
            beg++;
        else if (*beg == BRACE_OPEN)
            nest++;
        else if (*beg == BRACE_CLOSE)
            nest--;

        if (NOT_AT_END(beg, end)) /* in case '\\' is the last character */
            beg++;

        if (nest == 0 && NOT_AT_END(beg, end))  /* we are at outer level and not at end */
            return false;
    }

    /* we can check only if there are too many opening delimiters */
    /* this should not happen anyway */
    if (nest > 0)
    {
        tintin_eprintf(ses, "Unmatched braces error - too many '%c'", BRACE_OPEN);
        return false;
    }

    return true;
}

/* FUNCTION:  simplify_list - removes unwanted characters from        */
/*                            beginning and end of string             */
/* ARGUMENTS: *beg - points to the first character of list            */
/*            *end - points to the element after last (usually '\0')  */
/*            flag - should we remove outside braces if possible?     */
/* RESULT:    modifies *beg and/or *end such that spaces from         */
/*            the beginning and the end of list are removed and if    */
/*            list contains single braced atom (see is_braced_atom    */
/*            for further description) then group delimiters are      */
/*            removed - similar to #getitem command behavior          */
/*            in the case like this                                   */
/*            If you don't like this behavior simply undefine         */
/*            REMOVE_ONEELEM_BRACES.                                  */
/*            see also: getitemnr_command, REMOVE_ONEELEM_BRACES      */
static void simplify_list(char **beg, char **end, bool flag, struct session *ses)
{
    /* remember: we do not check arguments (e.g. if they are not NULL) */

    /* removing spaces */
    /* we use 'isaspace' because 'space_out' does */
    while ((**beg) && ((*beg) < (*end)) && isaspace(**beg))
        (*beg)++;
    while (((*end) > (*beg)) && isaspace(*(*end - 1)))
        (*end)--;

#ifdef REMOVE_ONEELEM_BRACES
    /* removing unnecessary braces (one pair at most) */
    if (flag&&is_braced_atom_2((*beg), (*end), ses))
    {
        (*beg)++;
        (*end)--;
    }
#endif
}

/* FUNCTION:  copy_part - string copying                 */
/* ARGUMENTS: dest - destination string                  */
/*            beg - points to first character to copy    */
/*            end - points after last character to copy  */
/* RESULT:    zero-ended string from beg to end          */
static char* copy_part(char *dest, char *beg, char *end)
{
    strcpy(dest, beg);
    dest[end - beg] = '\0';
    return dest;
}

/* FUNCTION:  split_list - main function                                   */
/* ARGUMENTS: list - list to split                                         */
/*            head_length - number of elements head will have              */
/* RESULT:    head - head of the list (elements 1..head_length             */
/*            tail - tail of the list (elements head_length+1..list_size)  */
/* NOTE:      both head and tail could be empty strings;                   */
/*            if head or/and tail contains only one element                */
/*            and REMOVE_ONEELEM_BRACES is defined the element is unbraced */
/*            if necessary                                                 */
static void split_list(char *head, char *tail, char *list, int head_length, struct session *ses)
{
    /* these are pointers, not strings */
    char *headbeg, *headend;
    char *tailbeg, *tailend;

    if (!*list)
    {
        head[0] = tail[0] = '\0';
        return;
    }

    /* where to split */
    tailbeg = get_split_pos(list, head_length);
    /* head */
    headbeg = list;
    headend = tailbeg;

    simplify_list(&headbeg, &headend, 1, ses);
    copy_part(head, headbeg, headend);
    /* tail */
    tailend = tailbeg + strlen(tailbeg);
    simplify_list(&tailbeg, &tailend, 0, ses);
    copy_part(tail, tailbeg, tailend);
}

/**************************/
/* the #splitlist command */
/**************************/
void splitlist_command(const char *arg, struct session *ses)
{
    /* command arguments */
    char headvar[BUFFER_SIZE], tailvar[BUFFER_SIZE];
    char list[BUFFER_SIZE];
    char headlengthtxt[BUFFER_SIZE];
    /* temporary variables */
    char head[BUFFER_SIZE], tail[BUFFER_SIZE];
    int head_length;


    /** get command arguments */
    arg = get_arg(arg, headvar, 0, ses);
    arg = get_arg(arg, tailvar, 0, ses);

    if (!*headvar && !*tailvar)
        return tintin_eprintf(ses, "#Error - Syntax: #splitlist {head variable} {tail variable} "
                     "{list} [{head size}]");

    arg = get_arg(arg, list, 1, ses);
    arg = get_arg(arg, headlengthtxt, 1, ses);

    if (!*headlengthtxt)
    {
        /* defaults number of elements in head to 1 if no value given */
        headlengthtxt[0] = '1'; headlengthtxt[1] = '\0';
        head_length = 1;
    }
    else
    {
        if (sscanf(headlengthtxt, "%d", &head_length) != 1)
            return tintin_eprintf(ses, "#Error in #splitlist - head size has to be number>=0, got {%s}.", headlengthtxt);
        if (head_length < 0)
            return tintin_eprintf(ses, "#Error in #splitlist - head size could not be negative, got {%d}.", head_length);
    }

    /** invoke main procedure **/
    split_list(head, tail, list, head_length, ses);

    /** do all assignments **/
    if (*headvar) /* nonempty names only */
        set_variable(headvar, head, ses);
    if (*tailvar) /* nonempty names only */
        set_variable(tailvar, tail, ses);
}

/****************************/
/* the #deleteitems command */
/****************************/
void deleteitems_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], list[BUFFER_SIZE], item[BUFFER_SIZE],
    temp[BUFFER_SIZE], right[BUFFER_SIZE], *lpos, *rpos;

    arg = get_arg(arg, left, 0, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Error - Syntax: #deleteitem {dest. variable} {list} {item}");

    arg=get_arg(arg, list, 0, ses);
    get_arg(arg, item, 1, ses);
    arg = list;
    rpos=right;
    if (*arg)
        do {
            arg = get_arg_in_braces(arg, temp, 0);
            if (!match(item, temp))
            {
                if (rpos!=right)
                    *rpos++=' ';
                lpos=temp;
                if (isatom(temp))
                    while (*lpos)
                        *rpos++=*lpos++;
                else
                {
                    *rpos++=BRACE_OPEN;
                    while (*lpos)
                        *rpos++=*lpos++;
                    *rpos++=BRACE_CLOSE;
                }
            }
        } while (*arg);
    *rpos=0;
    set_variable(left, right, ses);
}

/**************************/
/* the #foreach command   */
/**************************/
struct session *foreach_command(const char *arg, struct session *ses)
{
    char temp[BUFFER_SIZE], left[BUFFER_SIZE], right[BUFFER_SIZE];
    const char *p, *list;
    pvars_t vars, *lastvars;

    arg=get_arg(arg, left, 0, ses);
    get_arg_in_braces(arg, right, 1);
    if (!*right)
    {
        tintin_eprintf(ses, "#SYNTAX: foreach {list} command");
        return ses;
    }
    lastvars=pvars;
    pvars=&vars;
    list = left;
    while (*list)
    {
        list = get_arg_in_braces(list, temp, 0);
        strcpy(vars[0], p=temp);
        for (int i=1;i<10;i++)
            p=get_arg_in_braces(p, vars[i], 0);
        in_alias=true;
        ses=parse_input(right, true, ses);
    }
    pvars=lastvars;
    return ses;
}

struct session *forall_command(const char *arg, struct session *ses)
{
    return foreach_command(arg, ses);
}

static int compar(const void *a, const void *b)
{
    return strcmp(*(char **)a, *(char **)b);
}

/***************************/
/* the #sortlist command   */
/***************************/
void sortlist_command(const char *arg, struct session *ses)
{
    char *list, temp[BUFFER_SIZE], left[BUFFER_SIZE], right[BUFFER_SIZE];
    char *tab[BUFFER_SIZE];

    arg=get_arg(arg, left, 0, ses);
    get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#SYNTAX: sortlist var {list}");

    int n=0;
    arg = right;
    while (*arg)
    {
        arg = get_arg_in_braces(arg, temp, 0);
        tab[n++]=mystrdup(temp);
    }
    qsort(tab, n, sizeof(char*), compar);
    list=temp;
    for (int i=0;i<n;i++)
    {
        if (list!=temp)
            *list++=' ';
        if (*tab[i]&&isatom(tab[i]))
            list+=sprintf(list, "%s", tab[i]);
        else
            list+=sprintf(list, "{%s}", tab[i]);
    }
    *list=0;
    set_variable(left, temp, ses);
}


/************************/
/* the #explode command */
/************************/
void explode_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], del[BUFFER_SIZE], right[BUFFER_SIZE],
         *p, *n, *r,
         res[3*BUFFER_SIZE]; /* worst case is ",,,,," -> "{} {} {} {} {} {}" */

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, del, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left || !*del)
        return tintin_eprintf(ses, "#Syntax: #explode <var> <delimiter> <text>");

    r=res;
    p=right;
    while ((n=strstr(p, del)))
    {
        *n=0;
        if (p!=right)
            *r++=' ';
        if (*p&&isatom(p))
            r+=sprintf(r, "%s", p);
        else
            r+=sprintf(r, "{%s}", p);
        p=n+strlen(del);
    }
    if (p!=right)
        *r++=' ';
    if (*p&&isatom(p))
        r+=sprintf(r, "%s", p);
    else
        r+=sprintf(r, "{%s}", p);

    if (strlen(res)>BUFFER_SIZE-10)
    {
        tintin_eprintf(ses, "#ERROR: exploded line too long in #explode {%s} {%s} {%s}", left, del, right);
        res[BUFFER_SIZE-10]=0;
    }
    set_variable(left, res, ses);
}


/************************/
/* the #implode command */
/************************/
void implode_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], del[BUFFER_SIZE], right[BUFFER_SIZE],
         res[BUFFER_SIZE], temp[BUFFER_SIZE];
    const char *p;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, del, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left || !*del)
        return tintin_eprintf(ses, "#Syntax: #implode <var> <delimiter> <list>");

    int dellen=strlen(del);
    p = get_arg_in_braces(right, res, 0);
    int len=strlen(res);
    char *r = res+len;
    while (*p)
    {
        p = get_arg_in_braces(p, temp, 0);
        if ((len+=strlen(temp)+dellen) > BUFFER_SIZE-10)
        {
            tintin_eprintf(ses, "#ERROR: imploded line too long in #implode {%s} {%s} {%s}", left, del, right);
            break;
        }
        r+=sprintf(r, "%s%s", del, temp);
    }
    set_variable(left, res, ses);
}


/************************/
/* the #collate command */
/************************/
void collate_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], list[BUFFER_SIZE],
         cur[BUFFER_SIZE], last[BUFFER_SIZE], out[BUFFER_SIZE],
         *outptr, *err;

    arg = get_arg(arg, left, 0, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Error - Syntax: #collate {dest var} {list}");

    strcpy(last, "#X~4~~2~~12~[This is a sentinel!]~7~X");
    *(outptr=out)=0;
    get_arg(arg, list, 1, ses);
    arg = list;
    int i=0;
    while (*arg)
    {
        int j;
        arg=space_out(arg);
        if (isadigit(*arg))
            j=strtol(arg, &err, 10), arg=err;
        else
            j=1;
        if (!*arg || *arg==' ')
            continue;
        arg = get_arg_in_braces(arg, cur, 0);
        if (!j)
            continue;
        if (strcmp(cur, last))
        {
            if (i>1)
                outptr+=sprintf(outptr, "%d", i);
            if (i)
                outptr+=sprintf(outptr, isatom(last)?"%s ":"{%s} ", last);
            strcpy(last, cur);
            i=j;
        }
        else
            i+=j;
    }
    if (i>1)
        outptr+=sprintf(outptr, "%d", i);
    if (i)
        outptr+=sprintf(outptr, isatom(last)?"%s":"{%s}", last);
    set_variable(left, out, ses);
}


/***********************/
/* the #expand command */
/***********************/
void expand_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], list[BUFFER_SIZE],
         cur[BUFFER_SIZE], out[BUFFER_SIZE], *outptr, *err;
    int j;

    arg = get_arg(arg, left, 0, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Error - Syntax: #expand {dest var} {list}");

    *(outptr=out)=0;
    get_arg(arg, list, 1, ses);
    arg = list;
    while (*arg)
    {
        arg=space_out(arg);
        if (isadigit(*arg))
            j=strtol(arg, &err, 10), arg=err;
        else
            j=1;
        if (!*arg || *arg==' ')
            continue;
        arg = get_arg_in_braces(arg, cur, 0);
        if (j>BUFFER_SIZE/2)
                j=BUFFER_SIZE/2;
        for (int i=0;i<j;i++)
        {
            if (isatom(cur))
                outptr+=snprintf(outptr, out+BUFFER_SIZE-outptr, " %s", cur);
            else
                outptr+=snprintf(outptr, out+BUFFER_SIZE-outptr, " {%s}", cur);
            if (outptr>=out+BUFFER_SIZE-1)
                return tintin_eprintf(ses, "#ERROR: expanded line too long in {%s}", list);
        }
    }
    set_variable(left, (*out)?out+1:out, ses);
}


/******************************/
/* the #reverselist command   */
/******************************/
void reverselist_command(const char *arg, struct session *ses)
{
    char temp[BUFFER_SIZE], left[BUFFER_SIZE], right[BUFFER_SIZE];
    char *tab[BUFFER_SIZE];

    arg=get_arg(arg, left, 0, ses);
    get_arg(arg, right, 1, ses);
    if (!*left)
        return tintin_eprintf(ses, "#SYNTAX: reverselist var {list}");

    int n=0;
    arg = right;
    while (*arg)
    {
        arg = get_arg_in_braces(arg, temp, 0);
        tab[n++]=mystrdup(temp);
    }

    char *list=temp;
    for (int i=n-1;i>=0;i--)
    {
        if (list!=temp)
            *list++=' ';
        if (*tab[i]&&isatom(tab[i]))
            list+=sprintf(list, "%s", tab[i]);
        else
            list+=sprintf(list, "{%s}", tab[i]);
        free(tab[i]);
    }
    *list=0;
    set_variable(left, temp, ses);
}


/*****************************/
/* the findvariables command */
/*****************************/
void findvariables_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    if (!ses)
        return tintin_eprintf(ses, "#NO SESSION ACTIVE => NO VARS!");

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
    {
        tintin_eprintf(ses, "#Syntax: #findvariables <result> <pattern>");
        return;
    }

    if (!*right)
        strcpy(right, "*");

    if (is_literal(right))
    {
        if (get_hash(ses->myvars, right))
            set_variable(left, right, ses);
        else
            set_variable(left, "", ses);
        return;
    }
    // Otherwise, need to scan the whole hash.

    char buf[BUFFER_SIZE], *b=buf;
    *buf=0;

    struct pairlist *pl = hash2list(ses->myvars, right);
    struct pair *end = &pl->pairs[0] + pl->size;
    for (struct pair *nptr = &pl->pairs[0]; nptr<end; nptr++)
    {
        if (b!=buf)
            *b++=' ';
        if (isatom(nptr->left))
            b+=snprintf(b, buf-b+sizeof(buf), "%s", nptr->left);
        else
            b+=snprintf(b, buf-b+sizeof(buf), "{%s}", nptr->left);
        if (b >= buf+BUFFER_SIZE-10)
        {
            tintin_eprintf(ses, "#Too many variables to store.");
            break;
        }
    }

    set_variable(left, buf, ses);
    free(pl);
}
