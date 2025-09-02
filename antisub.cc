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
#include "protos/utils.h"
#include "protos/vars.h"


/*******************************/
/* the #antisubstitute command */
/*******************************/
void antisubstitute_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE], tmp[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, tmp, 1);
    substitute_myvars(tmp, left, ses, 0);

    if (!*left)
    {
        tintin_printf(ses, "#THESE ANTISUBSTITUTES HAVE BEEN DEFINED:");
        for (auto p : ses->antisubs)
            tintin_printf(0, "~7~{%s~7~}", p);
    }
    else
    {
        if (!ses->antisubs.count(left))
            ses->antisubs.insert(mystrdup(left));
        antisubnum++;
        if (ses->mesvar[MSG_SUBSTITUTE])
            tintin_printf(ses, "Ok. Any line with {%s} will not be subbed.", left);
#ifdef HAVE_SIMD
        ses->antisubs_dirty=true;
#endif
    }
}


void unantisubstitute_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE];
    bool had_any = false;

    get_arg_in_braces(arg, left, 1);

    if (strchr(left, '*')) /* wildcard deletion -- have to check all */
    {
        std::erase_if(ses->antisubs, [&](char* p)
        {
            if (!match(left, p))
                return false;
            had_any = true;
            if (ses->mesvar[MSG_SUBSTITUTE])
                tintin_printf(ses, "#Ok. Lines with {%s} will now be subbed.", p);
            SFREE(p);
            return true;
        });
    }
    else /* single item deletion */
    {
        std::set<char*>::iterator str = ses->antisubs.find(left);
        if (str != ses->antisubs.end())
        {
            had_any = true;
            if (ses->mesvar[MSG_SUBSTITUTE])
                tintin_printf(ses, "#Ok. Lines with {%s} will now be subbed.", left);
            SFREE(*str);
            ses->antisubs.erase(str);
        }
    }

#ifdef HAVE_SIMD
    if (had_any)
        ses->antisubs_dirty=true;
#endif
    if (!had_any && ses->mesvar[MSG_SUBSTITUTE])
        tintin_printf(ses, "#THAT ANTISUBSTITUTE (%s) IS NOT DEFINED.", left);
}


#ifdef HAVE_SIMD
static void build_antisubs_hs(session *ses)
{
    hs_free_database(ses->antisubs_hs);
    ses->antisubs_hs=0;
    ses->antisubs_dirty=false;

    int n = ses->antisubs.size();
    auto pat = new const char *[n];
    auto flags = new unsigned int[n];
    if (!pat || !flags)
        die("out of memory");

    int j=0;
    for (auto p : ses->antisubs)
    {
        pat[j]=action_to_regex(p);
        flags[j]=HS_FLAG_DOTALL|HS_FLAG_SINGLEMATCH;
        j++;
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
    delete[] flags;
    delete[] pat;

    if (ses->antisubs_hs && hs_alloc_scratch(ses->antisubs_hs, &hs_scratch))
        die("out of memory");
}

static int anti_match(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *context)
{
    return 1;
}
#endif


bool do_one_antisub(const char *line, session *ses)
{
#ifdef HAVE_SIMD
    if (ses->antisubs.empty())
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
    for (auto p : ses->antisubs)
        if (check_one_action(line, p, &vars, false))
            return true;
    return false;
}
