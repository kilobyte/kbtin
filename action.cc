/*********************************************************************/
/* file: action.c - functions related to the action command          */
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
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/tlist.h"
#include "protos/vars.h"


static int var_len[10];
static const char *var_ptr[10];

const char *match_start, *match_end;

extern session *if_command(const char *arg, session *ses) __attribute__((nonnull));
static bool check_a_action(const char *line, const char *action, bool inside);

static bool delete_acts(Acts &l, const char *pat, const char *msg, session *ses)
{
    return std::erase_if(l, [&](const auto& i)
    {
        if (match(pat, i.first.second.c_str()))
        {
            ses->actpr[l == ses->actions].erase(i.first.second);
            tintin_printf(MSG_ACTION, ses, msg, i.first.second.c_str());
            return true;
        }

        return false;
    });
}

static bool show_acts(Acts &acts, const char *pat, session *ses)
{
    bool had_any = false;

    for (const auto &a : acts)
    {
        if (pat && !match(pat, a.first.second.c_str()))
            continue;
        had_any = true;
        tintin_printf(ses, "~7~{%s~7~}={%s~7~} @ {%s}",
            a.first.second.c_str(), a.second.c_str(), a.first.first.c_str());
    }

    return had_any;
}

/***********************/
/* the #action command */
/***********************/
static void parse_action(const char *arg, session *ses, Acts &l, const char *what)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    char pr[BUFFER_SIZE];
    bool isact = l == ses->actions;
    auto &actpr = ses->actpr[isact];

    arg = get_arg_in_braces(arg, right, 0);
    substitute_myvars(right, left, ses, 0);
    arg = get_arg_in_braces(arg, right, 1);
    arg = get_arg_in_braces(arg, pr, 1);
    if (!*pr)
        strcpy(pr, "5"); /* defaults priority to 5 if no value given */
    if (!*left)
    {
        tintin_printf(ses, "#Defined %ss:", what);
        show_acts(l, 0, ses);
    }
    else if (*left && !*right)
    {
        if (!show_acts(l, left, ses))
            tintin_printf(MSG_ACTION, ses, "#That %s (%s) is not defined.", what, left);
    }
    else
    {
        const auto &op = actpr.find(left);
        if (op != actpr.cend())
            l.erase(Strpair(pr, left));
        actpr[left] = pr;
        l.emplace(Strpair(pr, left), right);
        tintin_printf(MSG_ACTION, ses, "#Ok. {%s} now triggers {%s} @ {%s}", left, right, pr);
        acnum++;
#ifdef HAVE_SIMD
        ses->act_dirty[isact] = true;
#endif
    }
}

void action_command(const char *arg, session *ses)
{
    parse_action(arg, ses, ses->actions, "action");
}

/*****************************/
/* the #promptaction command */
/*****************************/
void promptaction_command(const char *arg, session *ses)
{
    parse_action(arg, ses, ses->prompts, "promptaction");
}


/*************************/
/* the #unaction command */
/*************************/
void unaction_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 1);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #unaction <pattern>");

    if (!delete_acts(ses->actions, left, "#Ok. {%s} is no longer an action.", ses))
    {
        tintin_printf(MSG_ACTION, ses, "#No match(es) found for {%s}", left);
        return;
    }

#ifdef HAVE_SIMD
    ses->act_dirty[1] = true;
#endif
}

/*******************************/
/* the #unpromptaction command */
/*******************************/
void unpromptaction_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 1);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #unpromptaction <pattern>");

    if (!delete_acts(ses->prompts, left, "#Ok. {%s} is no longer a promptaction.", ses))
    {
        tintin_printf(MSG_ACTION, ses, "#No match(es) found for {%s}", left);
        return;
    }

#ifdef HAVE_SIMD
    ses->act_dirty[0] = true;
#endif
}


#ifdef HAVE_SIMD
char *action_to_regex(const char *pat)
{
    char buf[BUFFER_SIZE*2+3], *b=buf;

    assert(*pat);
    if (*pat=='^')
        *b++='^', pat++;
    for (; *pat; pat++)
        switch (*pat)
        {
        case '%':
            if (isadigit(pat[1]))
            {
                pat++;
                *b++='.';
                *b++='*';
                while (pat[1]=='%' && isadigit(pat[2]))
                    pat++; // eat multiple *s
                break;
            }
        case '\\':
        case '^':
        case '+':
        case '*':
        case '?':
        case '[':
        case ']':
        case '(':
        case ')':
        case '|':
            *b++='\\';
        default:
            *b++=*pat;
        break;
        case '$':
            if (pat[1])
                *b++='\\';
            *b++='$';
        }
    *b=0;

    int len=b-buf+1;
    char *txt = static_cast<char*>(malloc(len));
    memcpy(txt, buf, len);
    return txt;
}
#endif


/**********************************************/
/* check actions from a sessions against line */
/**********************************************/
static void check_all_act_serially(const char *line, session *ses, Acts &acts, bool act)
{
    pvars_t vars, *lastpvars;
    std::vector<Cstrpair> found;

    for (const auto& a : acts)
    {
        const char *left = a.first.second.c_str();
        if (check_a_action(line, left, false))
            found.emplace_back(Cstrpair(mystrdup(left), mystrdup(a.second)));
    }

    for (const auto& a : found)
    {
        if (check_one_action(line, a.first, &vars, false))
        {
            lastpvars = pvars;
            pvars = &vars;
            if (ses->mesvar[MSG_ACTION] && activesession == ses)
            {
                char buffer[BUFFER_SIZE];

                substitute_vars(a.second, buffer, ses);
                tintin_printf(ses, "[%sACTION: %s]", act? "":"PROMPT", buffer);
            }
            debuglog(ses, "%sACTION: {%s}->{%s}", act? "":"PROMPT", line, a.second);
            parse_input(a.second, true, ses);
            recursion=0;
            pvars = lastpvars;
        }

        SFREE(const_cast<char*>(a.first));
        SFREE(const_cast<char*>(a.second));
    }
}

#ifdef HAVE_SIMD
static void build_act_hs(Acts &acts, session *ses, bool act)
{
    debuglog(ses, "SIMD: building %sactions", act?"":"prompt");
    hs_free_database(ses->acts_hs[act]);
    ses->acts_hs[act]=0;
    ses->act_dirty[act]=false;
    delete[] ses->acts_data[act];
    ses->acts_data[act]=0;

    int n = acts.size();
    auto pat = new const char *[n];
    auto flags = new unsigned int[n];
    auto ids = new unsigned int[n];
    auto data = new Cstrpair[n];

    if (!pat || !flags || !ids || !data)
        die("out of memory");

    unsigned int j=0;
    for (const auto &a : acts)
    {
        pat[j]=action_to_regex(a.first.second.c_str());
        flags[j]=HS_FLAG_DOTALL|HS_FLAG_SINGLEMATCH|HS_FLAG_ALLOWEMPTY;
        ids[j]=j;
        const_cast<const char*&>(data[j].first) = a.first.second.c_str();
        data[j].second = a.second.c_str();
        j++;
    }

    debuglog(ses, "SIMD: compiling");
    hs_compile_error_t *error;
    if (hs_compile_multi(pat, flags, ids, j, HS_MODE_BLOCK, 0, &ses->acts_hs[act], &error))
    {
        tintin_eprintf(ses, "#Error: hyperscan compilation failed for %s: %s",
            act? "actions" : "promptactions", error->message);
        if (error->expression>=0)
            tintin_eprintf(ses, "#Converted bad expression was: %s",
                           pat[error->expression]);
        hs_free_compile_error(error);
    }
    else if (hs_alloc_scratch(ses->acts_hs[act], &hs_scratch))
        die("out of memory");

    ses->acts_data[act]=data;
    for (int i=0; i<n; i++)
        SFREE((char*)pat[i]);
    delete[] ids;
    delete[] flags;
    delete[] pat;
    debuglog(ses, "SIMD: rebuilt %sactions", act?"":"prompt");
}

static int act_match(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *context)
{
    auto nid = static_cast<unsigned **>(context);
    *(*nid)++=id;
    return 0;
}

static int uintcmp(const void *a, const void *b)
{
    unsigned A = *(const unsigned*)a;
    unsigned B = *(const unsigned*)b;
    return A>B? 1 : A<B? -1 : 0;
}

static void check_all_act_simd(const char *line, session *ses, Acts &acts, bool act)
{
    if (ses->act_dirty[act])
        build_act_hs(acts, ses, act);

    pvars_t vars, *lastpvars;
    lastpvars = pvars;
    pvars = &vars;

    auto ids = new unsigned[acts.size()];
    unsigned *nid=ids;
    hs_error_t err=hs_scan(ses->acts_hs[act], line, strlen(line), 0, hs_scratch,
        act_match, &nid);
    if (err)
    {
        tintin_eprintf(ses, "#Error in hs_scan: %d", err);
        delete[] ids;
        pvars = lastpvars;
        return;
    }

    int n = nid-ids;
    // Ensure a consistent ordering of actions.  While we don't guarantee
    // any order, in practice the order stays the same unless for a small
    // chance to change when the set of defined actions changes.
    // This leads to heisenbugs for the user, unfun.
    qsort(ids, n, sizeof(unsigned), uintcmp);

    // Copy all triggered actions, in case of a mutation.
    const Cstrpair *data = ses->acts_data[act];
    const auto trig = new Cstrpair[n];
    for (int i=0; i<n; i++)
    {
        const Strpair& t = data[ids[i]];
        const_cast<const char*&>(trig[i].first) = mystrdup(t.first);
        trig[i].second = mystrdup(t.second);
    }
    delete[] ids;

    for (int i=0; i<n; i++)
    {
        const auto& t = trig[i];
        const char *left  = t.first;
        const char *right = t.second;

        if (check_one_action(line, left, &vars, false))
        {
            if (ses->mesvar[MSG_ACTION] && activesession == ses)
            {
                char buffer[BUFFER_SIZE];

                substitute_vars(right, buffer, ses);
                tintin_printf(ses, "[%sACTION: %s]", act? "":"PROMPT", buffer);
            }
            debuglog(ses, "%sACTION: {%s}->{%s}", act? "":"PROMPT", line, right);
            parse_input(right, true, ses);
            recursion=0;
        }
    }
    for (int i=0; i<n; i++)
    {
        SFREE(const_cast<char*>(trig[i].first));
        SFREE(const_cast<char*>(trig[i].second));
    }
    delete[] trig;
    pvars = lastpvars;
}
#endif

static void check_all_act(const char *line, session *ses, bool act)
{
    Acts &acts = act? ses->actions : ses->prompts;
    if (acts.empty())
        return;

#ifdef HAVE_SIMD
    if (simd)
        check_all_act_simd(line, ses, acts, act);
    else
#endif
    check_all_act_serially(line, ses, acts, act);
}

void check_all_actions(const char *line, session *ses)
{
    check_all_act(line, ses, 1);
}

void check_all_promptactions(const char *line, session *ses)
{
    check_all_act(line, ses, 0);
}


/**********************/
/* the #match command */
/**********************/
session *match_command(const char *arg, session *ses)
{
    pvars_t vars, *lastpvars;
    char left[BUFFER_SIZE], line[BUFFER_SIZE], right[BUFFER_SIZE];

    arg=get_arg_in_braces(arg, left, 0);
    arg=get_arg(arg, line, 0, ses);
    arg=get_arg_in_braces(arg, right, 0);

    if (!*left || !*right)
    {
        tintin_eprintf(ses, "#ERROR: valid syntax is: #match <pattern> <line> <command> [#else ...]");
        return ses;
    }

    if (check_one_action(line, left, &vars, false))
    {
        lastpvars = pvars;
        pvars = &vars;
        ses = parse_input(right, true, ses);
        pvars = lastpvars;
        return ses;
    }

    return ifelse("match", arg, ses);
}


/*********************/
/* the #match inline */
/*********************/
int match_inline(const char *arg, session *ses)
{
    pvars_t vars;
    char left[BUFFER_SIZE], line[BUFFER_SIZE];

    arg=get_arg(arg, line, 0, ses);
    substitute_myvars(line, left, ses, 0); // no ivars
    arg=get_arg(arg, line, 1, ses);

    if (!*left)
    {
        tintin_eprintf(ses, "#ERROR: valid syntax is: (#match <pattern> <line>)");
        return 0;
    }

    return check_one_action(line, left, &vars, false);
}


static int match_a_string(const char *line, const char *mask)
{
    const char *lptr, *mptr;

    lptr = line;
    mptr = mask;
    while (*lptr && *mptr && !(*mptr == '%' && isadigit(*(mptr + 1))))
        if (*lptr++ != *mptr++)
            return -1;
    if (!*lptr && *mptr == '$' && !mptr[1])
        return (int)(lptr - line);
    if (!*mptr || (*mptr == '%' && isadigit(*(mptr + 1))))
        return (int)(lptr - line);
    return -1;
}

bool check_one_action(const char *line, const char *action, pvars_t *vars, bool inside)
{
    if (!check_a_action(line, action, inside))
        return false;

    for (int i = 0; i < 10; i++)
    {
        if (var_len[i] != -1)
        {
            memcpy((*vars)[i], var_ptr[i], var_len[i]);
            *((*vars)[i] + var_len[i]) = '\0';
        }
        else
            (*vars)[i][0]=0;
    }
    return true;
}

/******************************************************************/
/* check if a text triggers an action and fill into the variables */
/* return true if triggered                                       */
/******************************************************************/
static bool check_a_action(const char *line, const char *action, bool inside)
{
    const char *lptr, *lptr2, *tptr, *temp2;
    int len;
    bool flag_anchor = false;

    for (int i = 0; i < 10; i++)
        var_len[i] = -1;
    lptr = line;
    tptr = action;
    if (*tptr == '^')
    {
        if (inside)
            return false;
        tptr++;
        flag_anchor = true;
    }
    if (flag_anchor)
    {
        if ((len = match_a_string(lptr, tptr)) == -1)
            return false;
        match_start=lptr;
        lptr += len;
        tptr += len;
    }
    else
    {
        len = -1;
        do
            if ((len = match_a_string(lptr, tptr)) != -1)
                break;
            else
                lptr++;
        while (*lptr);
        if (len != -1)
        {
            match_start=lptr;
            lptr += len;
            tptr += len;
        }
        else
            return false;
    }
    while (*lptr && *tptr)
    {
        temp2 = tptr + 2;
        if (!*temp2 || *temp2=='$')
        {
            var_len[*(tptr + 1) - 48] = strlen(lptr);
            var_ptr[*(tptr + 1) - 48] = lptr;
            match_end=""; /* NOTE: to use (match_end-line) change this line */
            return true;
        }
        lptr2 = lptr;
        len = -1;
        while (*lptr2)
        {
            if ((len = match_a_string(lptr2, temp2)) != -1)
                break;
            else
                lptr2++;
        }
        if (len != -1)
        {
            var_len[*(tptr + 1) - 48] = lptr2 - lptr;
            var_ptr[*(tptr + 1) - 48] = lptr;
            lptr = lptr2 + len;
            tptr = temp2 + len;
        }
        else
            return false;
    }
    if ((*tptr=='%')&&isadigit(*(tptr+1)))
    {
        var_len[*(tptr+1)-48]=0;
        var_ptr[*(tptr+1)-48]=lptr;
        tptr+=2;
    }
    if (!*lptr && *tptr == '$' && !tptr[1])
        tptr++;
    match_end=lptr;
    return !*tptr;
}


void doactions_command(const char *arg, session *ses)
{
    char line[BUFFER_SIZE];

    get_arg(arg, line, 1, ses);
    /* the line provided may be empty */

    check_all_actions(line, ses);
}


void dopromptactions_command(const char *arg, session *ses)
{
    char line[BUFFER_SIZE];

    get_arg(arg, line, 1, ses);
    /* the line provided may be empty */

    check_all_promptactions(line, ses);
}
