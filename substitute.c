/*********************************************************************/
/* file: substitute.c - functions related to the substitute command  */
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
#include "protos/string.h"
#include "protos/tlist.h"
#include "protos/utils.h"
#include "protos/vars.h"


/***************************/
/* the #substitute command */
/***************************/
static void list_subs(const char *left, bool gag, struct session *ses)
{
    bool flag = false;
    kbtree_t(trip) *sub = ses->subs;

    TRIP_ITER(sub, mysubs)
        if (gag)
        {
            if (!strcmp(mysubs->right, EMPTY_LINE))
            {
                if (!flag)
                    tintin_printf(ses, "#THESE GAGS HAVE BEEN DEFINED:");
                tintin_printf(ses, "{%s~7~}", mysubs->left);
                flag=true;
            }
        }
        else
        {
            if (!flag)
                tintin_printf(ses, "#THESE SUBSTITUTES HAVE BEEN DEFINED:");
            flag=true;
            show_trip(mysubs);
        }
    ENDITER

    if (!flag && ses->mesvar[MSG_SUBSTITUTE])
    {
        if (strcmp(left, "*"))
            tintin_printf(ses, "#THAT %s IS NOT DEFINED.", gag? "GAG":"SUBSTITUTE");
        else
            tintin_printf(ses, "#NO %sS HAVE BEEN DEFINED.", gag? "GAG":"SUBSTITUTE");
    }
}

static void parse_sub(const char *left_, const char *right,  bool gag, struct session *ses)
{
    char left[BUFFER_SIZE];
    substitute_myvars(left_, left, ses, 0);

    if (!*right)
        return list_subs(*left? left : "*", gag, ses);

    kbtree_t(trip) *sub = ses->subs;

    ptrip new = MALLOC(sizeof(struct trip));
    new->left = mystrdup(left_);
    new->right = mystrdup(right);
    new->pr = 0;
    ptrip *t = kb_get(trip, sub, new);
    if (t)
    {
        ptrip d = *t;
        kb_del(trip, sub, new);
        free(d->left);
        free(d->right);
        free(d);
    }
    kb_put(trip, sub, new);
    subnum++;
    if (ses->mesvar[MSG_SUBSTITUTE])
    {
        if (strcmp(right, EMPTY_LINE))
            tintin_printf(ses, "#Ok. {%s} now replaces {%s}.", right, left);
        else
            tintin_printf(ses, "#Ok. {%s} is now gagged.", left);
    }
}

void substitute_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    arg = get_arg_in_braces(arg, left, 0);
    arg = get_arg_in_braces(arg, right, 1);

    parse_sub(left, right, 0, ses);
}

void gag_command(const char *arg, struct session *ses)
{
    char temp[BUFFER_SIZE];

    if (!*arg)
        return parse_sub("", "", 1, ses);
    get_arg_in_braces(arg, temp, 1);
    parse_sub(temp, EMPTY_LINE, 1, ses);
}

/*****************************/
/* the #unsubstitute command */
/*****************************/
static bool is_not_gag(char **right)
{
    return strcmp(*right, EMPTY_LINE);
}

static bool is_gag(char **right)
{
    return !strcmp(*right, EMPTY_LINE);
}

static void unsub(const char *arg, bool gag, struct session *ses)
{
    char left[BUFFER_SIZE];
    arg = get_arg_in_braces(arg, left, 1);

    if (!delete_tlist(ses->subs, left, ses->mesvar[MSG_SUBSTITUTE]?
        gag? "#Ok. {%s} is no longer gagged." :
        "#Ok. {%s} is no longer substituted." : 0,
        gag? is_not_gag : is_gag, true)
        && ses->mesvar[MSG_SUBSTITUTE])
    {
        tintin_printf(ses, "#THAT SUBSTITUTE (%s) IS NOT DEFINED.", left);
    }
}

void unsubstitute_command(const char *arg, struct session *ses)
{
    unsub(arg, false, ses);
}

void ungag_command(const char *arg, struct session *ses)
{
    unsub(arg, true, ses);
}

#define APPEND(srch)    if (rlen+len > BUFFER_SIZE-1)           \
                            len=BUFFER_SIZE-1-rlen;             \
                        memcpy(result+rlen, srch, len);               \
                        rlen+=len;

void do_all_sub(char *line, struct session *ses)
{
    pvars_t vars, *lastpvars;
    char result[BUFFER_SIZE], tmp[BUFFER_SIZE];
    const char *l;
    int rlen, len;

    lastpvars=pvars;
    pvars=&vars;

    TRIP_ITER(ses->subs, ln)
        if (check_one_action(line, ln->left, &vars, false))
        {
            if (!strcmp(ln->right, EMPTY_LINE))
            {
                strcpy(line, EMPTY_LINE);
                return;
            }
            substitute_vars(ln->right, tmp, ses);
            rlen=match_start-line;
            memcpy(result, line, rlen);
            len=strlen(tmp);
            APPEND(tmp);
            while (*match_end)
                if (check_one_action(l=match_end, ln->left, &vars, true))
                {
                    /* no gags possible here */
                    len=match_start-l;
                    APPEND(l);
                    substitute_vars(ln->right, tmp, ses);
                    len=strlen(tmp);
                    APPEND(tmp);
                }
                else
                {
                    len=strlen(l);
                    APPEND(l);
                    break;
                }
            memcpy(line, result, rlen);
            line[rlen]=0;
        }
    ENDITER

    pvars=lastpvars;
}

void changeto_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    if (!_)
        return tintin_eprintf(ses, "#ERROR: #change is allowed only inside an action/promptaction");

    get_arg(arg, left, 1, ses);
    strcpy(_, left);
}

void gagthis_command(const char *arg, struct session *ses)
{
    if (!_)
        return tintin_eprintf(ses, "#ERROR: #gagthis is allowed only inside an action/promptaction");

    strcpy(_, EMPTY_LINE);
}
