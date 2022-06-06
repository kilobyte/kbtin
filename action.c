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

static int inActions=0;
static bool mutatedActions;
static int deletedActions=0;
static char **stray_strings=0;
static int max_strays=0;
const char *match_start, *match_end;

extern struct session *if_command(const char *arg, struct session *ses);
static bool check_a_action(const char *line, const char *action, bool inside);

#ifdef HAVE_HS
struct acts
{
    char *pr;
    hs_database_t *hs;
    int n;
};

static int actcmp(void *a, void *b)
{
    return prioritycmp(((pacts)a)->pr, ((pacts)b)->pr);
}

/**/ KBTREE_CODE(acts, pacts, actcmp)

void kill_acts(struct session *ses, bool act)
{
    if (!ses->acts_hs[act])
        return;

    ACTS_ITER(ses->acts_hs[act], p)
        free(p->pr);
        hs_free_database(p->hs);
        free(p);
    ENDITER

    kb_destroy(acts, ses->acts_hs[act]);
    ses->acts_hs[act]=0;
}
#endif

static bool save_action(char **right)
{
    if (deletedActions==max_strays)
    {
        if (!max_strays)
            max_strays=16;
        else
            max_strays*=2;
        stray_strings = realloc(stray_strings, max_strays*sizeof(char*));
    }
    stray_strings[deletedActions] = *right;
    **right=0;
    *right=0;
    deletedActions++;

    return false;
}

static void zap_actions(void)
{
    for (int i=0;i<deletedActions;i++)
        free(stray_strings[i]);
    free(stray_strings);
    stray_strings=0;
    max_strays=0;
    deletedActions=0;
}

/***********************/
/* the #action command */
/***********************/
static void parse_action(const char *arg, struct session *ses, bool act, kbtree_t(trip) *l, const char *what)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    char pr[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, right, 0);
    substitute_myvars(right, left, ses, 0);
    arg = get_arg_in_braces(arg, right, 1);
    arg = get_arg_in_braces(arg, pr, 1);
    if (!*pr)
        strcpy(pr, "5"); /* defaults priority to 5 if no value given */
    if (!*left)
    {
        tintin_printf(ses, "#Defined %ss:", what);
        show_tlist(l, 0, 0);
    }
    else if (*left && !*right)
    {
        if (!show_tlist(l, left, 0) && ses->mesvar[MSG_ACTION])
            tintin_printf(ses, "#That %s (%s) is not defined.", what, left);
    }
    else
    {
        // FIXME: O(n²)
        TRIP_ITER(l, old)
            if (!strcmp(old->left, left))
            {
                kb_del(trip, l, old);
                free(old->left);
                free(old->right);
                free(old->pr);
                free(old);
                break;
            }
        ENDITER

        ptrip new = MALLOC(sizeof(struct trip));
        new->left = mystrdup(left);
        new->right = mystrdup(right);
        new->pr = mystrdup(pr);
        kb_put(trip, l, new);
        if (ses->mesvar[MSG_ACTION])
            tintin_printf(ses, "#Ok. {%s} now triggers {%s} @ {%s}", left, right, pr);
        acnum++;
        mutatedActions = true;
#ifdef HAVE_HS
        ses->act_dirty[l == ses->actions] = true;
#endif
    }
}

void action_command(const char *arg, struct session *ses)
{
    parse_action(arg, ses, 1, ses->actions, "action");
}

/*****************************/
/* the #promptaction command */
/*****************************/
void promptaction_command(const char *arg, struct session *ses)
{
    parse_action(arg, ses, 0, ses->prompts, "promptaction");
}


/*************************/
/* the #unaction command */
/*************************/
void unaction_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 1);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #unaction <pattern>");

    if (!delete_tlist(ses->actions, left, ses->mesvar[MSG_ACTION]?
            "#Ok. {%s} is no longer an action." : 0,
            inActions? save_action : 0, false)
        && ses->mesvar[MSG_ACTION])    /* is it an error or not? */
    {
        tintin_printf(ses, "#No match(es) found for {%s}", left);
    }

    mutatedActions = true;
#ifdef HAVE_HS
    ses->act_dirty[1] = true;
#endif
}

/*******************************/
/* the #unpromptaction command */
/*******************************/
void unpromptaction_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 1);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #unpromptaction <pattern>");

    if (!delete_tlist(ses->prompts, left, ses->mesvar[MSG_ACTION]?
            "#Ok. {%s} is no longer a promptaction." : 0,
            inActions? save_action : 0, false)
        && ses->mesvar[MSG_ACTION])    /* is it an error or not? */
    {
        tintin_printf(ses, "#No match(es) found for {%s}", left);
    }

    mutatedActions = true;
#ifdef HAVE_HS
    ses->act_dirty[0] = true;
#endif
}


#ifdef HAVE_HS
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
    char *txt=MALLOC(len);
    memcpy(txt, buf, len);
    return txt;
}
#endif


/**********************************************/
/* check actions from a sessions against line */
/**********************************************/
static void check_all_act_serially(const char *line, struct session *ses, kbtree_t(trip) *acts, bool act)
{
    pvars_t vars, *lastpvars;

    TRIP_ITER(acts, ln)
        if (!*ln->right) // marker for deleted actions
            continue;

        if (check_one_action(line, ln->left, &vars, false))
        {
            char mleft[BUFFER_SIZE], mpr[BUFFER_SIZE];
            strlcpy(mleft, ln->left, sizeof mleft);
            strlcpy(mpr, ln->pr, sizeof mpr);

            lastpvars = pvars;
            pvars = &vars;
            if (ses->mesvar[MSG_ACTION] && activesession == ses)
            {
                char buffer[BUFFER_SIZE];

                substitute_vars(ln->right, buffer, ses);
                tintin_printf(ses, "[%sACTION: %s]", act? "":"PROMPT", buffer);
            }
            debuglog(ses, "%sACTION: {%s}->{%s}", act? "":"PROMPT", line,
                ln->right);
            parse_input(ln->right, true, ses);
            recursion=0;
            pvars = lastpvars;

            if (mutatedActions)
            {
                mutatedActions=false;
                struct trip srch = {mleft, 0, mpr};
                kb_itr_after(trip, acts, &itr, &srch);
            }
        }
    ENDITER
}

#ifdef HAVE_HS
static void build_act_hs(kbtree_t(trip) *acts, struct session *ses, bool act)
{
    kill_acts(ses, act);
    ses->act_dirty[act]=false;
    free(ses->acts_data[act]);
    ses->acts_data[act]=0;
    ses->acts_hs[act] = kb_init(acts, KB_DEFAULT_SIZE);

    int n = count_tlist(acts);
    const char **pat = MALLOC(n*sizeof(void*));
    unsigned int *flags = MALLOC(n*sizeof(int));
    unsigned int *ids = MALLOC(n*sizeof(int));
    ptrip *data = MALLOC(n*sizeof(ptrip));

    if (!pat || !flags || !ids || !data)
        syserr("out of memory");

    int j=0, base=0;
    char *lastpr = 0;
    TRIP_ITER(acts, ln)
        if (!lastpr)
            lastpr=ln->pr;
        else if (strcmp(ln->pr, lastpr))
        {
last:;
            struct acts *a = MALLOC(sizeof(struct acts));
            a->pr = mystrdup(lastpr);
            a->n = j-base;

            hs_compile_error_t *error;
            if (hs_compile_multi(pat+base, flags+base, ids+base, j-base, HS_MODE_BLOCK,
                0, &a->hs, &error))
            {
                tintin_eprintf(ses, "#Error: hyperscan compilation failed for %s: %s",
                    act? "actions" : "promptactions", error->message);
                if (error->expression>=0)
                    tintin_eprintf(ses, "#Converted bad expression was: %s",
                                   pat[error->expression]);
                hs_free_compile_error(error);
            }
            else if (hs_alloc_scratch(a->hs, &hs_scratch))
                syserr("out of memory");
            kb_put(acts, ses->acts_hs[act], a);
            if (!acts)
                goto out;
            lastpr = ln->pr;
            base=j;
        }

        pat[j]=action_to_regex(ln->left);
        flags[j]=HS_FLAG_DOTALL|HS_FLAG_SINGLEMATCH|HS_FLAG_ALLOWEMPTY;
        ids[j]=j;
        data[j]=ln;
        j++;
    ENDITER
    acts=0;
    goto last;
out:
    ses->acts_data[act]=data;

    for (int i=0; i<n; i++)
        SFREE((char*)pat[i]);
    MFREE(ids, n*sizeof(int));
    MFREE(flags, n*sizeof(int));
    MFREE(pat, n*sizeof(void*));
}

static int act_match(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *context)
{
    unsigned **nid=context;
    *(*nid)++=id;
    return 0;
}

static void check_all_act_simd(const char *line, struct session *ses, kbtree_t(trip) *acts, bool act)
{
    if (ses->act_dirty[act])
        build_act_hs(acts, ses, act);

    pvars_t vars, *lastpvars;
    lastpvars = pvars;
    pvars = &vars;

    ACTS_ITER(ses->acts_hs[act], a)
        char mpr[BUFFER_SIZE];
        strlcpy(mpr, a->pr, sizeof mpr);
        unsigned *ids=malloc(a->n * sizeof(unsigned));
        unsigned *nid=ids;
        hs_error_t err=hs_scan(a->hs, line, strlen(line), 0, hs_scratch,
            act_match, &nid);
        if (err)
        {
            tintin_eprintf(ses, "#Error in hs_scan: %d\n", err);
            free(ids);
            continue;
        }

        // Copy all triggered actions, in case of a mutation.
        int n = nid-ids;
        ptrip *data=ses->acts_data[act];
        struct trip *trips=malloc(n*sizeof(struct trip));
        for (int i=0; i<n; i++)
        {
            ptrip ln = data[ids[i]];
            trips[i]=*ln;
        }
        free(ids);

        for (int i=0; i<n; i++)
        {
            struct trip ln = trips[i];
            if (!*ln.right) // marker for deleted
                continue;
            if (check_one_action(line, ln.left, &vars, false))
            {
                if (ses->mesvar[MSG_ACTION] && activesession == ses)
                {
                    char buffer[BUFFER_SIZE];

                    substitute_vars(ln.right, buffer, ses);
                    tintin_printf(ses, "[%sACTION: %s]", act? "":"PROMPT", buffer);
                }
                debuglog(ses, "%sACTION: {%s}->{%s}", act? "":"PROMPT", line,
                    ln.right);
                parse_input(ln.right, true, ses);
                recursion=0;
            }
        }
        free(trips);

        if (mutatedActions)
        {
            mutatedActions=false;
            struct acts srch = {mpr, 0, 0};
            kb_itr_after(acts, ses->acts_hs[act], &itr, &srch);
        }
    ENDITER
    pvars = lastpvars;
}
#endif

static void check_all_act(const char *line, struct session *ses, bool act)
{
    kbtree_t(trip) *acts = act? ses->actions : ses->prompts;
    if (!kb_size(acts))
        return;

    inActions++;
    bool oldMutated = mutatedActions;
    mutatedActions = false;

#ifdef HAVE_HS
    if (simd)
        check_all_act_simd(line, ses, acts, act);
    else
#endif
    check_all_act_serially(line, ses, acts, act);

    mutatedActions = oldMutated;
    inActions--;
    if (deletedActions && !inActions)
        zap_actions();
}

void check_all_actions(const char *line, struct session *ses)
{
    check_all_act(line, ses, 1);
}

void check_all_promptactions(const char *line, struct session *ses)
{
    check_all_act(line, ses, 0);
}


/**********************/
/* the #match command */
/**********************/
void match_command(const char *arg, struct session *ses)
{
    pvars_t vars, *lastpvars;
    char left[BUFFER_SIZE], line[BUFFER_SIZE], right[BUFFER_SIZE];
    bool flag=false;

    arg=get_arg_in_braces(arg, left, 0);
    arg=get_arg(arg, line, 0, ses);
    arg=get_arg_in_braces(arg, right, 0);

    if (!*left || !*right)
        return tintin_eprintf(ses, "#ERROR: valid syntax is: #match <pattern> <line> <command> [#else ...]");

    if (check_one_action(line, left, &vars, false))
    {
        lastpvars = pvars;
        pvars = &vars;
        parse_input(right, true, ses);
        pvars = lastpvars;
        flag=true;
    }
    arg=get_arg_in_braces(arg, left, 0);
    if (*left == tintin_char)
    {
        if (is_abrev(left + 1, "else"))
        {
            get_arg_in_braces(arg, right, 1);
            if (!flag)
                parse_input(right, true, ses);
            return;
        }
        if (is_abrev(left + 1, "elif"))
        {
            if (!flag)
                if_command(arg, ses);
            return;
        }
    }
    if (*left)
        tintin_eprintf(ses, "#ERROR: cruft after #match: {%s}", left);
}


/*********************/
/* the #match inline */
/*********************/
int match_inline(const char *arg, struct session *ses)
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
        while (*lptr)
            if ((len = match_a_string(lptr, tptr)) != -1)
                break;
            else
                lptr++;
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
        if (!*temp2)
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


void doactions_command(const char *arg, struct session *ses)
{
    char line[BUFFER_SIZE];

    get_arg(arg, line, 1, ses);
    /* the line provided may be empty */

    check_all_actions(line, ses);
}


void dopromptactions_command(const char *arg, struct session *ses)
{
    char line[BUFFER_SIZE];

    get_arg(arg, line, 1, ses);
    /* the line provided may be empty */

    check_all_promptactions(line, ses);
}
