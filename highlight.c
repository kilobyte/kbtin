#include "tintin.h"
#include "protos/action.h"
#include "protos/colors.h"
#include "protos/files.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/tlist.h"
#include "protos/vars.h"


static struct colordef
{
    int num, mask;
    const char *name;
} cNames[]=
    {
        { 0, CBG_MASK|CFL_MASK, "black"},
        { 1, CBG_MASK|CFL_MASK, "blue"},
        { 2, CBG_MASK|CFL_MASK, "green"},
        { 3, CBG_MASK|CFL_MASK, "cyan"},
        { 4, CBG_MASK|CFL_MASK, "red"},
        { 5, CBG_MASK|CFL_MASK, "magenta"},
        { 5, CBG_MASK|CFL_MASK, "violet"},
        { 6, CBG_MASK|CFL_MASK, "brown"},
        { 7, CBG_MASK|CFL_MASK, "grey"},
        { 7, CBG_MASK|CFL_MASK, "gray"},
        { 7, CBG_MASK|CFL_MASK, "normal"},
        { 8, CBG_MASK|CFL_MASK, "faint"}, // should be halfbright
        { 8, CBG_MASK|CFL_MASK, "charcoal"},
        { 8, CBG_MASK|CFL_MASK, "dark"},
        { 9, CBG_MASK|CFL_MASK, "light blue"},
        {10, CBG_MASK|CFL_MASK, "light green"},
        {11, CBG_MASK|CFL_MASK, "light cyan"},
        {11, CBG_MASK|CFL_MASK, "aqua"},
        {12, CBG_MASK|CFL_MASK, "light red"},
        {13, CBG_MASK|CFL_MASK, "light magenta"},
        {13, CBG_MASK|CFL_MASK, "pink"},
        {14, CBG_MASK|CFL_MASK, "yellow"},
        {15, CBG_MASK|CFL_MASK, "white"},
        {15, CBG_MASK|CFL_MASK, "bold"},
        {7<<CBG_AT, CFL_MASK, "inverse"}, // should keep but invert existing color
        {7<<CBG_AT, CFL_MASK, "reverse"}, // ditto
        {CFL_BLINK, C_MASK, "blink"},
        {CFL_UNDERLINE, C_MASK, "underline"},
        {CFL_UNDERLINE, C_MASK, "underlined"},
        {CFL_ITALIC, C_MASK, "italic"},
        {CFL_STRIKETHRU, C_MASK, "strike"},
        {-1, 0},
    };

static int highpattern[64], highmask[64];
static int nhighpattern;

static bool get_high_num(const char *hig, int hn)
{
    char tmp[BUFFER_SIZE];

    if (!*hig)
        return false;
    if (isadigit(*hig) || *hig==':')
    {
        const char *sl=strchr(hig, '/');
        if (!sl)
            sl=strchr(hig, 0);
        sprintf(tmp, "~%.*s~", (int)(sl-hig), hig);
        sl=tmp;
        int c=7;
        if (getcolor(&sl, &c, 0))
        {
            int minc=0;
            sl=tmp; getcolor(&sl, &minc, 0);
            int maxc=C_MASK;
            sl=tmp; getcolor(&sl, &maxc, 0);
            int mask = maxc &~ minc;
            highmask[hn] = mask;
            highpattern[hn] = c &~ mask;

            return true;
        }
    }
    for (int code=0;cNames[code].num!=-1;code++)
        if (is_abrev(hig, cNames[code].name))
        {
            highpattern[hn]=cNames[code].num;
            highmask[hn]=cNames[code].mask;
            return true;
        }
    return false;
}

static bool get_high(const char *hig)
{
    int nh=0;
    if (!*hig)
        return false;
    while (hig&&*hig)
    {
        if (!get_high_num(hig, nh++))
            return false;
        if ((hig=strchr(hig, '/')))
            hig++;
        if (nh==ARRAYSZ(highpattern))
            break;
    }
    nhighpattern = nh;
    return true;
}

static void show_high_help(struct session *ses)
{
    char buf[BUFFER_SIZE], *bp=buf;
    buf[0]=0;

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

/***************************/
/* the #highlight command  */
/***************************/
void highlight_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    char *pright, *bp, *cp, buf[BUFFER_SIZE];

    pright = right;
    *pright = '\0';
    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
    {
        tintin_printf(ses, "#THESE HIGHLIGHTS HAVE BEEN DEFINED:");
        show_tlist(ses->highs, 0, 0, true, ses);
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
        bp = cp + 1;

        if (!get_high(buf))
        {
            if (!puts_echoing && ses->mesvar[MSG_ERROR])
                return tintin_eprintf(ses, "#Invalid highlighting color: {%s}", left);

            if (strcmp(left, "list"))
                tintin_printf(ses, "#Invalid highlighting color, valid colors are:");
            show_high_help(ses);
            return;
        }
    }

    if (!*right)
    {
        if (ses->mesvar[MSG_HIGHLIGHT] || ses->mesvar[MSG_ERROR])
            tintin_eprintf(ses, "#Highlight WHAT?");
        return;
    }

    ptrip new = MALLOC(sizeof(struct trip));
    new->left = mystrdup(right);
    new->right = mystrdup(left);
    new->pr = 0;
    ptrip *old = kb_get(trip, ses->highs, new);
    if (old)
    {
        ptrip del = *old;
        kb_del(trip, ses->highs, new);
        free(del->left);
        free(del->right);
        free(del);
    }
    kb_put(trip, ses->highs, new);
    hinum++;
#ifdef HAVE_SIMD
    ses->highs_dirty = true;
#endif
    if (ses->mesvar[MSG_HIGHLIGHT])
        tintin_printf(ses, "#Ok. {%s} is now highlighted %s.", right, left);
}

/*****************************/
/* the #unhighlight command */
/*****************************/
void unhighlight_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg_in_braces(arg, left, 1);
    substitute_vars(left, left, ses);
    if (!*left)
        return tintin_eprintf(ses, "#Syntax: #unhighlight <pattern>");

    if (delete_tlist(ses->highs, left, ses->mesvar[MSG_HIGHLIGHT]?
            "#Ok. {%s} is no longer highlighted." : 0, 0, true, ses))
    {
#ifdef HAVE_SIMD
        ses->highs_dirty = true;
#endif
    }
    else if (ses->mesvar[MSG_ACTION])
        tintin_printf(ses, "#THAT HIGHLIGHT IS NOT DEFINED.");
}


static void apply_high(int *attr, int len)
{
    for (int i=0; i<len; i++)
    {
        int c = i % nhighpattern;
        int oldattr = (attr[i]==-1)? 7 : attr[i];
        attr[i] = (oldattr & highmask[c]) | highpattern[c];
    }
}

#ifdef HAVE_SIMD
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
        case '|':
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
    debuglog(ses, "SIMD: building highs");
    hs_free_database(ses->highs_hs);
    ses->highs_hs=0;
    ses->highs_dirty=false;
    free(ses->highs_cols);
    ses->highs_cols=0;

    int n = count_tlist(ses->highs);
    const char **pat = MALLOC(n*sizeof(void*));
    unsigned int *flags = MALLOC(n*sizeof(int));
    unsigned int *ids = MALLOC(n*sizeof(int));
    const char **cols = MALLOC(n*sizeof(void*));
    if (!pat || !flags || !ids || !cols)
        syserr("out of memory");

    int j=0;
    TRIP_ITER(ses->highs, ln)
        pat[j]=glob_to_regex(ln->left);;
        flags[j]=HS_FLAG_DOTALL|HS_FLAG_SOM_LEFTMOST;
        ids[j]=j;
        cols[j]=ln->right;
        j++;
    ENDITER
    ses->highs_cols=cols;

    debuglog(ses, "SIMD: compiling highs");
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
    debuglog(ses, "SIMD: rebuilt highs");

    for (int i=0; i<n; i++)
        SFREE((char*)pat[i]);
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
    apply_high(gattr + from, to - from);
    return 0;
}
#endif

// converts a ~X~ colored string into character:attribute pairs, returns length
static int attributize_colors(char *restrict text, int *restrict attr, const char *restrict line, int init)
{
    int c, d;
    const char *pos;
    char *txt;
    int *atr;

    c=init;
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

    return txt-text;
}

static void deattributize_colors(char *restrict line, const char *restrict text, const int *restrict attr, int init)
{
    int c=init;
    char *pos=line;
    const char *txt=text;
    const int *atr=attr;
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

void do_all_high(char *line, struct session *ses)
{
    if (!count_tlist(ses->highs))
        return;

#ifdef HAVE_SIMD
    if (simd && ses->highs_dirty)
        build_highs_hs(ses);
#endif

    char text[BUFFER_SIZE];
    int attr[BUFFER_SIZE];
    int len = attributize_colors(text, attr, line, -1);
    (void)len; // return value unused if !HAVE_SIMD

#ifdef HAVE_SIMD
    if (ses->highs_hs)
    {
        gattr = attr;
        hs_error_t err=hs_scan(ses->highs_hs, text, len, 0, hs_scratch,
                               high_match, ses);
        // TODO: optimize coloring multiple matches
        if (err)
            tintin_eprintf(ses, "#Error in hs_scan: %d", err);
        goto done;
    }
#endif

    TRIP_ITER(ses->highs, ln)
        char *txt=text;
        int l, r;
        while (*txt&&find(txt, ln->left, &l, &r, ln->pr))
        {
            if (!get_high(ln->right))
                break;
            r+=txt-text;
            l+=txt-text;
            /* changed: no longer highlight in the middle of a word */
            if (((l==0)||(!is7alnum(text[l])||!is7alnum(text[l-1])))&&
                    (!is7alnum(text[r])||!is7alnum(text[r+1])))
            {
                apply_high(attr+l, r+1-l);
            }
            txt=text+r+1;
        }
    ENDITER

#ifdef HAVE_SIMD
done:
#endif

    deattributize_colors(line, text, attr, -1);
}

/*************************/
/* the #colorize command */
/*************************/
void colorize_command(const char *arg, struct session *ses)
{
    char var[BUFFER_SIZE], color[BUFFER_SIZE], line[BUFFER_SIZE];

    arg = get_arg(arg, var, 0, ses);
    arg = get_arg(arg, color, 0, ses);
    arg = get_arg(arg, line, 1, ses);

    if (!*var)
        return tintin_eprintf(ses, "#Usage: #colorize <dest.var> <color> <text>");

    if (*color && !get_high(color))
    {
        tintin_eprintf(ses, "#Invalid highlighting color, valid ones are:");
        show_high_help(ses);
        return;
    }

    char text[BUFFER_SIZE];
    int attr[BUFFER_SIZE];

    int len=attributize_colors(text, attr, line, 7);
    // Currently old attributes are thrown away, but it'd be nice to allow
    // adding only a particular part (underline, foreground, background, etc)
    // while keeping the rest unmodified.

    // Go back to ~7~, ~-1~ is an internal hack.
    attr[len]=7;

    if (*color)
        apply_high(attr, len);

    deattributize_colors(line, text, attr, 7);
    set_variable(var, line, ses);
}
