/*********************************************************************/
/* file: substitute.c - functions related to the substitute command  */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include "protos/action.h"
#include "protos/files.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/string.h"
#include "protos/utils.h"
#include "protos/vars.h"


/***************************/
/* the #substitute command */
/***************************/
static void list_subs(const char *left, bool gag, session *ses)
{
    bool any = false;

    for (auto& s : ses->subs)
    {
        if (!match(left, s.first))
            continue;
        if (gag)
        {
            if (!strcmp(s.second, EMPTY_LINE))
            {
                if (!any)
                    tintin_printf(ses, "#THESE GAGS HAVE BEEN DEFINED:");
                tintin_printf(ses, "{%s~7~}", s.first);
                any=true;
            }
        }
        else
        {
            if (!any)
                tintin_printf(ses, "#THESE SUBSTITUTES HAVE BEEN DEFINED:");
            any=true;
            tintin_printf(ses, "~7~{%s~7~}={%s~7~}", s.first, s.second);
        }
    }

    if (any)
        return;

    if (strcmp(left, "*"))
        tintin_printf(ses, "#THAT %s IS NOT DEFINED.", gag? "GAG":"SUBSTITUTE");
    else
        tintin_printf(ses, "#NO %sS HAVE BEEN DEFINED.", gag? "GAG":"SUBSTITUTE");
}

static void parse_sub(const char *left_, const char *right,  bool gag, session *ses)
{
    char left[BUFFER_SIZE];
    substitute_myvars(left_, left, ses, 0);

    if (!*right)
        return list_subs(*left? left : "*", gag, ses);

    char *a = mystrdup(left);
    char *b = mystrdup(right);
    auto res = ses->subs.try_emplace(a, b);
    if (!res.second)
    {
        const auto& old = &*res.first;
        SFREE(a);
        SFREE(const_cast<char*>(old->second));
        old->second = b;
    }
    subnum++;
#ifdef HAVE_SIMD
    ses->subs_dirty=true;
#endif
    if (ses->mesvar[MSG_SUBSTITUTE])
    {
        if (strcmp(right, EMPTY_LINE))
            tintin_printf(ses, "#Ok. {%s} now replaces {%s}.", right, left);
        else
            tintin_printf(ses, "#Ok. {%s} is now gagged.", left);
    }
}

void substitute_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    arg = get_arg_in_braces(arg, left, 0);
    arg = get_arg_in_braces(arg, right, 1);

    parse_sub(left, right, 0, ses);
}

void gag_command(const char *arg, session *ses)
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
static bool delete_subs(const char *pat, const char *msg, bool gag, session *ses)
{
    if (is_literal(pat))
    {
        const auto& d = ses->subs.find(pat);
        if (d == ses->subs.cend())
            return false;
        if (gag != !strcmp(d->second, EMPTY_LINE))
            return false;
        if (msg)
            tintin_printf(ses, msg, d->first);
        SFREE(const_cast<char*>(d->first));
        SFREE(const_cast<char*>(d->second));
        ses->subs.erase(d);
        return true;
    }

    return erase_if(ses->subs, [&](const auto& i)
    {
        if (pat && !match(pat, i.first))
            return false;
        if (gag != !strcmp(i.second, EMPTY_LINE))
            return false;
        SFREE(const_cast<char*>(i.first));
        SFREE(const_cast<char*>(i.second));
        return true;
    });
}

static void unsub(const char *arg, bool gag, session *ses)
{
    char left[BUFFER_SIZE];
    arg = get_arg_in_braces(arg, left, 1);
    const char *msg = ses->mesvar[MSG_SUBSTITUTE]?
        gag? "#Ok. {%s} is no longer gagged." :
        "#Ok. {%s} is no longer substituted." : 0;

    if (!delete_subs(left, msg, gag, ses))
    {
        if (ses->mesvar[MSG_SUBSTITUTE])
            tintin_printf(ses, "#THAT SUBSTITUTE (%s) IS NOT DEFINED.", left);
    }

#ifdef HAVE_SIMD
    ses->subs_dirty=true;
#endif
}

void unsubstitute_command(const char *arg, session *ses)
{
    unsub(arg, false, ses);
}

void ungag_command(const char *arg, session *ses)
{
    unsub(arg, true, ses);
}

#define APPEND(srch)    if (rlen+len > BUFFER_SIZE-1)           \
                            len=BUFFER_SIZE-1-rlen;             \
                        memcpy(result+rlen, srch, len);               \
                        rlen+=len;

// returns true if gagged
static bool do_one_sub(char *line, const char* const left, const char *right, session *ses)
{
    char result[BUFFER_SIZE], tmp[BUFFER_SIZE];
    const char *l;
    int rlen, len;

    if (!check_one_action(line, left, pvars, false))
        return false;

    if (!strcmp(right, EMPTY_LINE))
    {
        strcpy(line, EMPTY_LINE);
        return true;
    }
    substitute_vars(right, tmp, ses);
    rlen=match_start-line;
    memcpy(result, line, rlen);
    len=strlen(tmp);
    APPEND(tmp);
    while (*match_end)
        if (check_one_action(l=match_end, left, pvars, true))
        {
            /* no gags possible here */
            len=match_start-l;
            APPEND(l);
            substitute_vars(right, tmp, ses);
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
    return false;
}

static void do_all_sub_serially(char *line, session *ses)
{
    pvars_t vars, *lastpvars;
    lastpvars=pvars;
    pvars=&vars;

    for (auto& s : ses->subs)
        if (do_one_sub(line, s.first, s.second, ses))
            break;

    pvars=lastpvars;
}

#ifdef HAVE_SIMD
// Hyperscan disallows patterns that match an empty buffer, we do them by hand.
static bool is_omni_regex(const char *pat)
{
    if (*pat=='^')
        pat++;
    if (pat[0]=='.' && pat[1]=='*') // we merge .* so there may be only one
        pat+=2;
    if (*pat=='$')
        pat++;
    return !*pat;
}

static void build_subs_hs(session *ses)
{
    debuglog(ses, "SIMD: building subs");
    hs_free_database(ses->subs_hs);
    ses->subs_hs=0;
    ses->subs_dirty=false;
    free(ses->subs_data);
    ses->subs_data=0;
    free(ses->subs_markers);
    ses->subs_markers=0;

    int n = ses->subs.size();
    auto pat = new const char*[n];
    auto flags = new unsigned int[n];
    auto ids = new unsigned int[n];
    auto data = new pCstrpair[n];
    auto markers = new uintptr_t[n];

    if (!pat || !flags || !ids || !data || !markers)
        die("out of memory");

    ses->subs_omni_last = ses->subs_omni_first = n;

    unsigned int j=0;
    for (auto& s : ses->subs)
    {
        pat[j]=action_to_regex(s.first);
        if (is_omni_regex(pat[j]))
        {
            pat[--ses->subs_omni_first] = pat[j];
            data[ses->subs_omni_first] = &s;
            continue;
        }
        flags[j] = HS_FLAG_DOTALL|HS_FLAG_SOM_LEFTMOST;
        ids[j]   = j;
        data[j]  = &s;
        j++;
    }
    ses->subs_data=data;
    bzero(markers, n*sizeof(uintptr_t));
    ses->subs_markers=markers;

    if (!ses->subs_omni_first)
        goto done;

    debuglog(ses, "SIMD: compiling subs");
    hs_compile_error_t *error;
    if (hs_compile_multi(pat, flags, ids, ses->subs_omni_first, HS_MODE_BLOCK,
        0, &ses->subs_hs, &error))
    {
        tintin_eprintf(ses, "#Error: hyperscan compilation failed for substitutes: %s",
            error->message);
        if (error->expression>=0)
            tintin_eprintf(ses, "#Converted bad expression was: %s",
                           pat[error->expression]);
        hs_free_compile_error(error);
    }

done:
    debuglog(ses, "SIMD: rebuilt subs");
    for (int i=0; i<n; i++)
        SFREE((char*)pat[i]);
    delete[] ids;
    delete[] flags;
    delete[] pat;

    if (ses->subs_hs && hs_alloc_scratch(ses->subs_hs, &hs_scratch))
        die("out of memory");
}

static unsigned short longest_len[BUFFER_SIZE];
static unsigned int longest_id[BUFFER_SIZE];

static int sub_match(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *context)
{
    if (from<BUFFER_SIZE && from<to)
        if (longest_len[from] < to-from)
        {
            longest_len[from] = to-from;
            longest_id[from] = id;
        }
    return 0;
}

static void do_all_sub_simd(char *line, session *ses)
{
    if (ses->subs_dirty)
        build_subs_hs(ses);

    if (!ses->subs_hs) // compilation error
        return do_all_sub_serially(line, ses);

    int len = strlen(line);
    for (int i=0; i<len; i++)
        longest_len[i]=0;
    for (int i=0; i<len; i++)
        longest_id[i]=0;

    hs_error_t err=hs_scan(ses->subs_hs, line, len, 0, hs_scratch,
                           sub_match, ses);
    if (err)
        return tintin_eprintf(ses, "#Error in hs_scan: %d", err);

    pvars_t vars, *lastpvars;
    lastpvars=pvars;
    pvars=&vars;

    static uintptr_t marker;
    marker++;

    for (int i=0; i<len; i++)
    {
        if (!longest_len[i])
            continue;
        pCstrpair ln = ses->subs_data[longest_id[i]];
        if (ses->subs_markers[longest_id[i]] == marker)
            continue; // already done
        if (do_one_sub(line, ln->first, ln->second, ses))
            goto gagged;
        ses->subs_markers[longest_id[i]] = marker;
    }

    for (int i=ses->subs_omni_first; i<ses->subs_omni_last; i++)
        if (do_one_sub(line, ses->subs_data[i]->first, ses->subs_data[i]->second, ses))
            goto gagged;
gagged:

    pvars=lastpvars;
}
#endif

void do_all_sub(char *line, session *ses)
{
#ifdef HAVE_SIMD
    if (ses->subs.empty())
        return;

    if (simd)
        return do_all_sub_simd(line, ses);
#endif

    do_all_sub_serially(line, ses);
}

void changeto_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE];

    if (!_)
        return tintin_eprintf(ses, "#ERROR: #change is allowed only inside an action/promptaction");

    get_arg(arg, left, 1, ses);
    strcpy(_, left);
}

void gagthis_command(const char *arg, session *ses)
{
    if (!_)
        return tintin_eprintf(ses, "#ERROR: #gagthis is allowed only inside an action/promptaction");

    strcpy(_, EMPTY_LINE);
}
