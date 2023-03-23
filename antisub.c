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
        tintin_printf(ses, "#THESE ANTISUBSTITUTES HAVE BEEN DEFINED:");
        show_slist(ass);
    }
    else
    {
        if (!kb_get(str, ass, left))
            kb_put(str, ass, mystrdup(left));
        antisubnum++;
        if (ses->mesvar[MSG_SUBSTITUTE])
            tintin_printf(ses, "Ok. Any line with {%s} will not be subbed.", left);
#ifdef HAVE_HS
        ses->antisubs_dirty=true;
#endif
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
        char **todel = malloc(kb_size(ass) * sizeof(char*));
        char **last = todel;

        STR_ITER(ass, p)
            if (match(left, p))
                *last++ = (char*)p;
        ENDITER

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
        char **str = kb_get(str, ass, left);
        if ((had_any = !!str))
        {
            if (ses->mesvar[MSG_SUBSTITUTE])
                tintin_printf(ses, "#Ok. Lines with {%s} will now be subbed.", left);
            kb_del(str, ass, left);
            free(*str);
        }
    }

#ifdef HAVE_HS
    if (had_any)
        ses->antisubs_dirty=true;
#endif
    if (!had_any && ses->mesvar[MSG_SUBSTITUTE])
        tintin_printf(ses, "#THAT ANTISUBSTITUTE (%s) IS NOT DEFINED.", left);
}


#ifdef HAVE_HS
static void build_antisubs_hs(struct session *ses)
{
    hs_free_database(ses->antisubs_hs);
    ses->antisubs_hs=0;
    ses->antisubs_dirty=false;

    int n = kb_size(ses->antisubs);
    const char **pat = MALLOC(n*sizeof(void*));
    unsigned int *flags = MALLOC(n*sizeof(int));
    if (!pat || !flags)
        syserr("out of memory");

    int j=0;
    {
        STR_ITER(ses->antisubs, p)
            pat[j]=action_to_regex(p);
            flags[j]=HS_FLAG_DOTALL|HS_FLAG_SINGLEMATCH;
            j++;
        ENDITER
    }

    hs_compile_error_t *error;
    if (hs_compile_multi(pat, flags, 0, n, HS_MODE_BLOCK,
        0, &ses->antisubs_hs, &error))
    {
        tintin_eprintf(ses, "#Error: hyperscan compilation failed for antisubs: %s",
            error->message);
        if (error->expression>=0)
            tintin_eprintf(ses, "#Converted bad expression was: %s",
                           pat[error->expression]);
        hs_free_compile_error(error);
    }

    for (int i=0; i<n; i++)
        SFREE((char*)pat[i]);
    MFREE(flags, n*sizeof(int));
    MFREE(pat, n*sizeof(void*));

    if (ses->antisubs_hs && hs_alloc_scratch(ses->antisubs_hs, &hs_scratch))
        syserr("out of memory");
}

static int anti_match(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *context)
{
    return 1;
}
#endif


bool do_one_antisub(const char *line, struct session *ses)
{
#ifdef HAVE_HS
    if (!kb_size(ses->antisubs))
        return false;

    if (simd && ses->antisubs_dirty)
        build_antisubs_hs(ses);

    if (simd)
    {
        hs_error_t err=hs_scan(ses->antisubs_hs, line, strlen(line), 0,
                               hs_scratch, anti_match, ses);
        if (err == HS_SCAN_TERMINATED)
            return true;
        if (!err)
            return false;
        tintin_eprintf(ses, "#Error in hs_scan: %d", err);
    }
#endif

    pvars_t vars;
    STR_ITER(ses->antisubs, p)
        if (check_one_action(line, p, &vars, false))
            return true;
    ENDITER
    return false;
}
