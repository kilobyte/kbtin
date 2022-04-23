#include "tintin.h"
#include "protos/action.h"
#include "protos/colors.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/llist.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/vars.h"
#include <assert.h>


static struct colordef
{
    int num;
    const char *name;
} cNames[]=
    {
        { 0, "black"},
        { 1, "blue"},
        { 2, "green"},
        { 3, "cyan"},
        { 4, "red"},
        { 5, "magenta"},
        { 5, "violet"},
        { 6, "brown"},
        { 7, "grey"},
        { 7, "gray"},
        { 7, "normal"},
        { 8, "faint"},
        { 8, "charcoal"},
        { 8, "dark"},
        { 9, "light blue"},
        {10, "light green"},
        {11, "light cyan"},
        {11, "aqua"},
        {12, "light red"},
        {13, "light magenta"},
        {13, "pink"},
        {14, "yellow"},
        {15, "white"},
        {15, "bold"},
        {7<<CBG_AT, "inverse"},
        {7<<CBG_AT, "reverse"},
        {CFL_BLINK, "blink"},
        {CFL_UNDERLINE, "underline"},
        {CFL_UNDERLINE, "underlined"},
        {CFL_ITALIC, "italic"},
        {CFL_STRIKETHRU, "strike"},
        {-1, 0},
    };

static int highpattern[64];
static int nhighpattern;

static int highcolor; /* an ugly kludge... */
static int get_high_num(const char *hig)
{
    char tmp[BUFFER_SIZE];

    if (!*hig)
        return -1;
    if (isadigit(*hig))
    {
        const char *sl=strchr(hig, '/');
        if (!sl)
            sl=strchr(hig, 0);
        sprintf(tmp, "~%.*s~", (int)(sl-hig), hig);
        sl=tmp;
        if (getcolor(&sl, &highcolor, 0))
            return highcolor;
    }
    for (int code=0;cNames[code].num!=-1;code++)
        if (is_abrev(hig, cNames[code].name))
            return highcolor=cNames[code].num;
    return -1;
}

static bool get_high(const char *hig)
{
    nhighpattern=0;
    if (!*hig)
        return false;
    while (hig&&*hig)
    {
        highcolor=7;
        if ((highpattern[nhighpattern++]=get_high_num(hig))==-1)
            return false;
        if ((hig=strchr(hig, '/')))
            hig++;
        if (nhighpattern==64)
            return true;
    }
    return true;
}

/***************************/
/* the #highlight command  */
/***************************/
void highlight_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    struct listnode *myhighs, *ln;
    bool colflag = true;
    char *pright, *bp, *cp, buf[BUFFER_SIZE];

    pright = right;
    *pright = '\0';
    myhighs = ses->highs;
    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
    {
        tintin_printf(ses, "#THESE HIGHLIGHTS HAVE BEEN DEFINED:");
        show_list(myhighs);
        return;
    }

    bp = left;
    cp = bp;
    while (*cp)
    {
        cp++;
        while (*cp != ',' && *cp)
            cp++;
        while (isaspace(*bp))
            bp++;
        memcpy(buf, bp, cp - bp);
        buf[cp - bp] = '\0';
        colflag = get_high(buf);
        bp = cp + 1;
    }
    if (colflag)
    {
        if (!*right)
        {
            if (ses->mesvar[MSG_HIGHLIGHT] || ses->mesvar[MSG_ERROR])
                tintin_eprintf(ses, "#Highlight WHAT?");
            return;
        }
        if ((ln = searchnode_list(myhighs, right)))
            deletenode_list(myhighs, ln);
        insertnode_list(myhighs, right, left, get_fastener(right, bp), LENGTH);
        hinum++;
#ifdef HAVE_HS
        ses->highs_dirty = true;
#endif
        if (ses->mesvar[MSG_HIGHLIGHT])
            tintin_printf(ses, "#Ok. {%s} is now highlighted %s.", right, left);
        return;
    }

    if (!puts_echoing && ses->mesvar[MSG_ERROR])
        return tintin_eprintf(ses, "#Invalid highlighting color: {%s}", left);

    if (strcmp(left, "list"))
        tintin_printf(ses, "#Invalid highlighting color, valid colors are:");
    buf[0]=0;
    bp=buf;
    for (int i=0;cNames[i].num!=-1;i++)
    {
        bp+=setcolor(bp, cNames[i].num);
        int len = sprintf(bp, "%s~7~, ", cNames[i].name);
        bp+=len;
        while (len++ < 21)
            *bp++=' ' ;
        *bp=0;
        if ((i%4)==3)
        {
            tintin_printf(ses, "%s", buf);
            buf[0]=0;
            bp=buf;
        }
    }
    tintin_printf(ses, "%sor 0..15:0..7:0..1", buf);
}

/*****************************/
/* the #unhighlight command */
/*****************************/

void unhighlight_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];
    struct listnode *myhighs, *ln, *temp;
    bool flag = false;

    myhighs = ses->highs;
    temp = myhighs;
    arg = get_arg_in_braces(arg, left, 1);
    substitute_vars(left, left, ses);
    while ((ln = search_node_with_wild(temp, left)))
    {
        if (ses->mesvar[MSG_HIGHLIGHT])
            tintin_printf(ses, "Ok. {%s} is no longer %s.", ln->left, ln->right);
        deletenode_list(myhighs, ln);
        flag = true;
#ifdef HAVE_HS
        ses->highs_dirty = true;
#endif
    }
    if (!flag && ses->mesvar[MSG_HIGHLIGHT])
        tintin_printf(ses, "#THAT HIGHLIGHT IS NOT DEFINED.");
}


#ifdef HAVE_HS
static char *glob_to_regex(const char *pat)
{
    char buf[BUFFER_SIZE*2+3], *b=buf;

    assert(*pat);
    if (*pat=='^')
        *b++='^', pat++;
    else if (is7alnum(*pat))
        *b++='\\', *b++='b';
    for (; *pat; pat++)
        switch (*pat)
        {
        case '*':
            *b++='.';
            *b++='*';
            while (pat[1]=='*')
                pat++; // eat multiple *s
            break;
        case '\\':
        case '^':
        case '$':
        case '+':
        case '?':
        case '[':
        case ']':
        case '(':
        case ')':
            *b++='\\';
        default:
            *b++=*pat;
        }
    if (is7alnum(pat[-1]))
        *b++='\\', *b++='b';
    *b=0;

    int len=b-buf+1;
    char *txt=MALLOC(len);
    memcpy(txt, buf, len);
    return txt;
}

static void build_highs_hs(struct session *ses)
{
    hs_free_database(ses->highs_hs);
    ses->highs_hs=0;
    ses->highs_dirty=false;
    free(ses->highs_cols);
    ses->highs_cols=0;

    int n = count_list(ses->highs);
    char **pat = MALLOC(n*sizeof(void*));
    unsigned int *flags = MALLOC(n*sizeof(int));
    unsigned int *ids = MALLOC(n*sizeof(int));
    const char **cols = MALLOC(n*sizeof(void*));
    if (!pat || !flags || !ids || !cols)
        syserr("out of memory");

    struct listnode *ln = ses->highs;
    int j=0;
    while ((ln=ln->next))
    {
        pat[j]=glob_to_regex(ln->left);;
        flags[j]=HS_FLAG_DOTALL|HS_FLAG_SOM_LEFTMOST;
        ids[j]=j;
        cols[j]=ln->right;
        j++;
    }
    ses->highs_cols=cols;

    hs_compile_error_t *error;
    if (hs_compile_multi(pat, flags, ids, n, HS_MODE_BLOCK,
        0, &ses->highs_hs, &error))
    {
        tintin_eprintf(ses, "#Error: hyperscan compilation failed for highlights: %s",
            error->message);
        if (error->expression>=0)
            tintin_eprintf(ses, "#Converted bad expression was: %s",
                           pat[error->expression]);
        hs_free_compile_error(error);
    }

    for (int i=0; i<n; i++)
        SFREE(pat[i]);
    MFREE(ids, n*sizeof(int));
    MFREE(flags, n*sizeof(int));
    MFREE(pat, n*sizeof(void*));

    if (ses->highs_hs && hs_alloc_scratch(ses->highs_hs, &hs_scratch))
        syserr("out of memory");
}

static int *gattr;
static int high_match(unsigned int id, unsigned long long from,
    unsigned long long to, unsigned int flags, void *context)
{
    struct session *ses = context;
    if (!get_high(ses->highs_cols[id]))
        return 0;
    int c=-1;
    for (unsigned long long i = from; i<to; i++)
        gattr[i]=highpattern[(++c)%nhighpattern];
    return 0;
}
#endif

void do_all_high(char *line, struct session *ses)
{
    if (!ses->highs->next)
        return;

#ifdef HAVE_HS
    if (simd && ses->highs_dirty)
        build_highs_hs(ses);
#endif

    char text[BUFFER_SIZE];
    int attr[BUFFER_SIZE];
    int c, d;
    char *pos, *txt;
    int *atr;
    int l, r;
    struct listnode *ln;

    c=-1;
    txt=text;
    atr=attr;
    for (pos=line;*pos;pos++)
    {
        if (getcolor((const char**)&pos, &d, 1))
            c=d;
        else
        {
            *txt++=*pos;
            *atr++=c;
        }
    };
    *txt=0;
    *atr=c;

#ifdef HAVE_HS
    if (ses->highs_hs)
    {
        gattr = attr;
        hs_error_t err=hs_scan(ses->highs_hs, text, txt-text, 0, hs_scratch,
                               high_match, ses);
        // TODO: optimize coloring multiple matches
        if (err)
            tintin_eprintf(ses, "#Error in hs_scan: %d\n", err);
        goto done;
    }
#endif

    ln=ses->highs;
    while ((ln=ln->next))
    {
        txt=text;
        while (*txt&&find(txt, ln->left, &l, &r, ln->pr))
        {
            if (!get_high(ln->right))
                break;
            c=-1;
            r+=txt-text;
            l+=txt-text;
            /* changed: no longer highlight in the middle of a word */
            if (((l==0)||(!isalnum(text[l])||!isalnum(text[l-1])))&&
                    (!isalnum(text[r])||!isalnum(text[r+1])))
                for (int i=l;i<=r;i++)
                    attr[i]=highpattern[(++c)%nhighpattern];
            txt=text+r+1;
        }
    }

#ifdef HAVE_HS
done:
#endif

    c=-1;
    pos=line;
    txt=text;
    atr=attr;
    while (*txt)
    {
        if (c!=*atr)
            pos+=setcolor(pos, c=*atr);
        *pos++=*txt++;
        atr++;
        if (pos - line > BUFFER_SIZE - 20)
        {
            while (*txt)
                txt++, atr++;
            break;
        }
    }
    if (c!=*atr)
        pos+=setcolor(pos, c=*atr);
    *pos=0;
}
