/*********************************************************************/
/* file: antisub.c - functions related to the substitute command     */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include "protos/action.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/slist.h"
#include "protos/utils.h"
#include "protos/vars.h"
#include "kbtree.h"

/*******************************/
/* the #antisubstitute command */
/*******************************/
void antisubstitute_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], tmp[BUFFER_SIZE];
    kbtree_t(str) *ass = ses->antisubs;

    arg = get_arg_in_braces(arg, tmp, 1);
    substitute_myvars(tmp, left, ses, 0);

    if (!*left)
    {
        tintin_puts("#THESE ANTISUBSTITUTES HAS BEEN DEFINED:", ses);
        show_slist(ass);
    }
    else
    {
        if (!kb_get(str, ass, left))
            kb_put(str, ass, mystrdup(left));
        antisubnum++;
        if (ses->mesvar[MSG_SUBSTITUTE])
            tintin_printf(ses, "Ok. Any line with {%s} will not be subbed.", left);
    }
}


void unantisubstitute_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];
    bool had_any = false;
    kbtree_t(str) *ass = ses->antisubs;

    get_arg_in_braces(arg, left, 1);

    if (strchr(left, '*')) /* wildcard deletion -- have to check all */
    {
        kbitr_t itr;
        char **todel = malloc(kb_size(ass) * sizeof(char*));
        char **last = todel;

        for (kb_itr_first(str, ass, &itr); kb_itr_valid(&itr); kb_itr_next(str, ass, &itr))
        {
            char *p = kb_itr_key(char*, &itr);
            if (match(left, p))
                *last++ = p;
        }

        if (last!=todel)
        {
            had_any = true;
            for (char **del = todel; del != last; del++)
            {
                if (ses->mesvar[MSG_SUBSTITUTE])
                    tintin_printf(ses, "#Ok. Lines with {%s} will now be subbed.", *del);
                kb_del(str, ass, *del);
                free(*del);
            }
        }
        free(todel);
    }
    else /* single item deletion */
    {
        char *str = *kb_get(str, ass, left);
        if ((had_any = !!str))
        {
            if (ses->mesvar[MSG_SUBSTITUTE])
                tintin_printf(ses, "#Ok. Lines with {%s} will now be subbed.", left);
            kb_del(str, ass, left);
            free(str);
        }
    }

    if (!had_any && ses->mesvar[MSG_SUBSTITUTE])
        tintin_printf(ses, "#THAT ANTISUBSTITUTE (%s) IS NOT DEFINED.", left);
}


bool do_one_antisub(const char *line, struct session *ses)
{
    kbtree_t(str) *ass = ses->antisubs;
    pvars_t vars;
    kbitr_t itr;

    for (kb_itr_first(str, ass, &itr); kb_itr_valid(&itr); kb_itr_next(str, ass, &itr))
    {
        const char *p = kb_itr_key(char*, &itr);
        if (check_one_action(line, p, &vars, false))
            return true;
    }
    return false;
}
