/*********************************************************************/
/* file: alias.c - functions related the the alias command           */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"


extern struct session *if_command(const char *arg, struct session *ses);

void show_hashlist(struct session *ses, struct hashtable *h, const char *pat, const char *msg_all, const char *msg_none)
{
    struct pairlist *pl;

    if (!*pat)
    {
        tintin_printf(ses, msg_all);
        pl=hash2list(h, 0);
    }
    else
        pl=hash2list(h, pat);
    if (pl->size)
    {
        struct pair *end = &pl->pairs[0] + pl->size;
        for (struct pair *n = &pl->pairs[0]; n<end; n++)
            tintin_printf(ses, "~7~{%s~7~}={%s~7~}", n->left, n->right);
    }
    else if (*pat)
        tintin_printf(ses, msg_none, pat);
    delete[] pl;
}

void delete_hashlist(struct session *ses, struct hashtable *h, const char *pat, const char *msg_ok, const char *msg_none)
{
    if (is_literal(pat))
    {
        if (delete_hash(h, pat))
        {
            if (msg_ok)
                tintin_printf(ses, msg_ok, pat);
        }
        else
        {
            if (msg_none)
                tintin_printf(ses, msg_none, pat);
        }
        return;
    }
    struct pairlist *pl = hash2list(h, pat);
    struct pair *end = &pl->pairs[0] + pl->size;
    for (struct pair *ln = &pl->pairs[0]; ln<end; ln++)
    {
        if (msg_ok)
            tintin_printf(ses, msg_ok, ln->left);
        delete_hash(h, ln->left);
    }
    if (msg_none && !pl->size)
        tintin_printf(ses, msg_none, pat);
    delete[] pl;
}


/**********************/
/* the #alias command */
/**********************/
void alias_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], *ch;

    arg = get_arg_in_braces(arg, left, 0);
    arg = get_arg_in_braces(arg, right, 1);

    if (*left && *right)
    {
        if ((ch=strchr(left, ' ')))
        {
            tintin_eprintf(ses, "#ERROR: aliases cannot contain spaces! Bad alias: {%s}", left);
            if (ch==left)
                return;
            *ch=0;
            tintin_printf(ses, "#Converted offending alias to {%s}.", left);
        }
        set_hash(ses->aliases, left, right);
        if (ses->mesvar[MSG_ALIAS])
            tintin_printf(ses, "#Ok. {%s} aliases {%s}.", left, right);
        alnum++;
        return;
    }
    show_hashlist(ses, ses->aliases, left,
        "#Defined aliases:",
        "#No match(es) found for {%s}.");
}

/************************/
/* the #unalias command */
/************************/
void unalias_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 1);
    delete_hashlist(ses, ses->aliases, left,
        ses->mesvar[MSG_ALIAS]? "#Ok. {%s} is no longer an alias." : 0,
        ses->mesvar[MSG_ALIAS]? "#No match(es) found for {%s}" : 0);
}

/******************************/
/* the #ifaliasexists command */
/******************************/
struct session *ifaliasexists_command(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], cmd[BUFFER_SIZE];

    line = get_arg(line, left, 0, ses);
    line = get_arg_in_braces(line, cmd, 1);

    if (!*cmd)
    {
        tintin_eprintf(ses, "#Syntax: #ifaliasexists <alias> <command> [#else <command>]");
        return ses;
    }

    if (get_hash(ses->aliases, left))
        return parse_input(cmd, true, ses);

    return ifelse("ifaliasexists", line, ses);
}
