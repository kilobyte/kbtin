/*********************************************************************/
/* file: path.c - stuff for the path feature                         */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                    coded by peter unold 1992                      */
/*                  recoded by Jeremy C. Jack 1994                   */
/*********************************************************************/
#include "tintin.h"
#include "protos/action.h"
#include "protos/alias.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/lists.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/path.h"
#include "protos/string.h"
#include "protos/utils.h"
#include "protos/vars.h"

#define PATHP(i) (&ses->path[(ses->path_begin+(i))%MAX_PATH_LENGTH])

static bool return_flag = true;

void mark_command(const char *arg, session *ses)
{
    ses->path_length = 0;
    tintin_printf(MSG_PATH, ses, "#Beginning of path marked.");
}

void map_command(const char *arg, session *ses)
{
    char cmd[BUFFER_SIZE];
    get_arg_in_braces(arg, cmd, 1);
    substitute_vars(cmd, cmd, ses);
    check_insert_path(cmd, ses);
}

void savepath_command(const char *arg, session *ses)
{
    char alias[BUFFER_SIZE], result[BUFFER_SIZE], *r=result;
    get_arg_in_braces(arg, alias, 1);
    if (!*arg)
        return tintin_eprintf(ses, "#Syntax: savepath <alias>");

    if (!ses->path_length)
    {
        tintin_eprintf(ses, "#No path to save!");
        return;
    }

    r+=snprintf(r, BUFFER_SIZE, "%calias {%s} {", tintin_char, alias);
    for (int i=0; i<ses->path_length; i++)
    {
        struct pair *ln = PATHP(i);
        int dirlen = strlen(ln->left);
        if (r-result + dirlen >= BUFFER_SIZE - 10)
        {
            tintin_eprintf(ses, "#Error - buffer too small to contain alias");
            break;
        }
        else
            r += sprintf(r, "%s%s", ln->left, (i==ses->path_length-1)?";":"");
    }
    *r++='}', *r=0;
    parse_input(result, true, ses);
}


void path2var(char *var, session *ses)
{
    char *r;
    int dirlen, len = 0;

    if (!ses->path_length)
    {
        *var=0;
        return;
    }

    len = 0;
    r=var;

    for (int i=0; i<ses->path_length; i++)
    {
        struct pair *ln = PATHP(i);
        dirlen = strlen(ln->left);
        if (dirlen + len < BUFFER_SIZE - 10)
        {
            r+=sprintf(r, isatom(ln->left)? "%s" : "{%s}" , ln->left);
            len += dirlen + 1;
            if (i<ses->path_length-1)
                *r++=' ';
        }
        else
        {
            tintin_eprintf(ses, "#Error - buffer too small to contain alias");
            *r++=0;
            break;
        }
    }
}


void path_command(const char *arg, session *ses)
{
    char mypath[BUFFER_SIZE];

    char *r=mypath+sprintf(mypath, "#Path:  ");
    for (int i=0; i<ses->path_length; i++)
    {
        struct pair *ln = PATHP(i);
        int dirlen = strlen(ln->left);
        if (r-mypath + dirlen > (COLS?COLS:BUFFER_SIZE)-10)
        {
            r[-1]=0;
            tintin_printf(ses, "%s", mypath);
            r=mypath+sprintf(mypath, "#Path:  ");
        }
        r+=sprintf(r, "%s;", ln->left);
    }
    r[-1]=0;
    tintin_printf(ses, "%s", mypath);
}

void return_command(const char *arg, session *ses)
{
    int n;
    char *err, how[BUFFER_SIZE];

    get_arg_in_braces(arg, how, 1);

    if (!ses->path_length)
        return tintin_eprintf(ses, "#No place to return from!");

    if (!*how)
        n=1;
    else if (!strcmp(how, "all") || !strcmp(how, "ALL"))
        n=ses->path_length;
    else
    {
        n=strtol(how, &err, 10);
        if (*err || n<0)
            return tintin_eprintf(ses, "#return [<num>|all], got {%s}", how);
        if (!n)     /* silently ignore "#return 0" */
            return;
    }

    return_flag = false; /* temporarily turn off path tracking */
    while (n--)
    {
        char command[BUFFER_SIZE];

        if (!ses->path_length)
            break;
        struct pair *ln = PATHP(--ses->path_length);
        strcpy(command, ln->right);
        parse_input(command, false, ses);
    }
    return_flag = true;  /* restore path tracking */
}

void unmap_command(const char *arg, session *ses)
{
    if (!ses->path_length)
        return tintin_eprintf(ses, "#No move to forget!");

    ses->path_length--;
    tintin_printf(MSG_PATH, ses, "#Ok.  Forgot that move.");
}

void check_insert_path(const char *command, session *ses)
{
    if (!return_flag)
        return;

    char *ret=get_hash(ses->pathdirs, command);
    if (!ret)
        return;
    if (ses->path_length != MAX_PATH_LENGTH)
        ses->path_length++;
    else
        ses->path_begin++;
    struct pair *p = PATHP(ses->path_length-1);
    free((char*)p->left);
    free((char*)p->right);
    p->left =mystrdup(command);
    p->right=mystrdup(ret);
}

void pathdir_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 0);
    arg = get_arg_in_braces(arg, right, 1);

    if (*left && *right)
    {
        set_hash(ses->pathdirs, left, right);
        tintin_printf(MSG_PATH, ses, "#Ok.  {%s} is marked in #path. {%s} is its #return.",
            left, right);
        pdnum++;
        return;
    }
    show_hashlist(ses, ses->pathdirs, left,
        "#These PATHDIRS have been defined:",
        "#That PATHDIR (%s) is not defined.");
}

void unpathdir_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 1);
    substitute_vars(left, left, ses);
    delete_hashlist(ses, ses->pathdirs, left,
        ses->mesvar[MSG_PATH]? "#Ok. $%s is no longer recognized as a pathdir." : 0,
        ses->mesvar[MSG_PATH]? "#THAT PATHDIR (%s) IS NOT DEFINED." : 0);
}
