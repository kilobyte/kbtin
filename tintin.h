/******************************************************************/
/* file: tintin.h - the include file for KBtin                    */
/******************************************************************/
#undef TELNET_DEBUG     /* define to show TELNET negotiations */
#undef USER_DEBUG       /* debugging of the user interface */
#undef KEYBOARD_DEBUG

#include <stdbool.h>

#define LOGCS_LOCAL     ((char*)1)
#define LOGCS_REMOTE    ((char*)2)

#define CFG_BITS	8
#define CBG_BITS	8
#define CFL_BITS	4

#define C_BLINK		1
#define C_ITALIC	2
#define C_UNDERLINE	4
#define C_STRIKETHRU	8

#define DENOM (1000000LL*3*3*3*7*2*5*2)

#define BIT_SHIFT	1
#define BIT_ALT		2
#define BIT_CTRL	4

/*************************/
/* telnet protocol stuff */
/*************************/
#define TERM                    "KBtin"   /* terminal type */

/************************************************************************/
/* Do you want to use help compression or not:  with it, space is saved */
/* but without it the help runs slightly faster.  If so, make sure the  */
/* compression stuff is defined in the default values.                  */
/************************************************************************/

#ifndef COMPRESSED_HELP
#define COMPRESSED_HELP 1
#endif

/***********************************************/
/* Some default values you might wanna change: */
/***********************************************/
#define CONSOLE_LENGTH 32768
#define STATUS_COLOR 0
#define INPUT_COLOR  (7 + (1<<CBG_BITS))
#define MARGIN_COLOR_ANSI 1
/* FIXME: neither INPUT_COLOR nor MARGIN_COLOR can be 7 */
/*#define IGNORE_INT*//* uncomment to disable INT (usually ^C) from keyboard */
#define XTERM_TITLE "KBtin - %s"
#undef  PTY_ECHO_HACK   /* not working yet */
#define ECHO_COLOR "~8~"
#define LOG_INPUT_PREFIX "" /* you can add ANSI codes, etc here */
#define LOG_INPUT_SUFFIX ""
#define RESET_RAW       /* reset pseudo-terminals to raw mode every write */
#define GOTO_CHAR '>'   /* be>mt -> #goto be mt */
                        /*Comment last line out to disable this behavior */
#define DEFAULT_LOGTYPE LOG_LF    /* LOG_RAW: cr/lf or what server sends, LOG_LF: lf, LOG_TTYREC */
#define BRACE_OPEN '{' /*character that starts an argument */
#define BRACE_CLOSE '}' /*character that ends an argument */
#define HISTORY_SIZE 128                  /* history size */
#define MAX_PATH_LENGTH 256               /* max path length (#route) */
#define DEFAULT_TINTIN_CHAR '#'           /* prefix for tintin commands */
#define DEFAULT_TICK_SIZE 60
#define DEFAULT_ROUTE_DISTANCE (10*DENOM)
#define VERBATIM_CHAR '\\'                /* if an input starts with this
                                             char, it will be sent 'as is'
                                             to the MUD */
#define MAX_SESNAME_LENGTH 512 /* don't accept session names longer than this */
#define MAX_RECURSION 64
#ifndef DEFAULT_FILE_DIR
#define DEFAULT_FILE_DIR "." /* Path to Tintin files, or HOME */
#endif
#if COMPRESSED_HELP
#define COMPRESSION_EXT ".gz"      /* for compress: ".Z" */
#define UNCOMPRESS_CMD "gzip -cd " /* for compress: "uncompress -c" */
#else
#define COMPRESSION_EXT ""
#endif
#define CONFIG_DIR ".tintin"
#define CERT_DIR   "ssl"

#define DEFAULT_DISPLAY_BLANK true        /* blank lines */
#define DEFAULT_ECHO_SEPINPUT true        /* echo when there is an input box */
#define DEFAULT_ECHO_NOSEPINPUT false     /* echo when input is not managed */
#define DEFAULT_IGNORE false              /* ignore */
#define DEFAULT_SPEEDWALK false           /* speedwalk */
        /* note: classic speedwalks are possible only on some primitive
           MUDs with only 4 basic directions (w,e,n,s)                   */
#define DEFAULT_PRESUB false              /* presub before actions */
#define DEFAULT_TOGGLESUBS false          /* turn subs on and off FALSE=ON*/
#define DEFAULT_KEYPAD false              /* start in standard keypad mode */
#define DEFAULT_RETAIN false              /* retain the last typed line */
#define DEFAULT_BOLD true                 /* allow terminals to make bright bold */
#define DEFAULT_ALIAS_MESS true           /* messages for responses */
#define DEFAULT_ACTION_MESS true          /* when setting/deleting aliases, */
#define DEFAULT_SUB_MESS true             /* actions, etc. may be set to */
#define DEFAULT_HIGHLIGHT_MESS true       /* default either on or off */
#define DEFAULT_VARIABLE_MESS true        /* might want to turn off these */
#define DEFAULT_EVENT_MESS true
#define DEFAULT_ROUTE_MESS true
#define DEFAULT_GOTO_MESS true
#define DEFAULT_BIND_MESS true
#define DEFAULT_SYSTEM_MESS true
#define DEFAULT_PATH_MESS true
#define DEFAULT_ERROR_MESS true
#define DEFAULT_HOOK_MESS true
#define DEFAULT_LOG_MESS true
#define DEFAULT_TICK_MESS true
#define DEFAULT_RATELIMIT_MESS true
#define DEFAULT_PRETICK 10
#define DEFAULT_CHARSET "UTF-8"           /* the MUD-side charset */
#define DEFAULT_LOGCHARSET LOGCS_LOCAL
#define DEFAULT_PARTIAL_LINE_MARKER 0
#define BAD_CHAR '?'                      /* marker for chars illegal for a charset */
#define CHAR_VERBATIM '\\'
#define CHAR_QUOTE '"'
#define CHAR_NEWLINE ';'
/*************************************************************************/
/* The text below is checked for. If it trickers then echo is turned off */
/* echo is turned back on the next time the user types a return          */
/*************************************************************************/
#define PROMPT_FOR_PW_TEXT "*assword:*"
#define PROMPT_FOR_PW_TEXT2 "*assphrase:*"
/*************************************************************************/
/* Whether the MUD tells us to echo off, let's check whether it's a      */
/* password input prompt, or a --More-- prompt.  Unfortunately, both     */
/* types can come in a variety of types ("*assword:", "Again:", national */
/* languages, etc), so it's better to assume it's a password if unsure.  */
/*************************************************************************/
#define PROMPT_FOR_MORE_TEXT "*line * of *"

#define REMOVE_ONEELEM_BRACES /* remove braces around one element list in
                                 #splitlist command i.e. {atom} -> atom
                                 similar to #getitemnr command behaviour */

#define EMPTY_LINE "-gag-"
#define STACK_LIMIT 8192*1024

#define BUFFER_SIZE 4096
#define INPUT_CHUNK 1536

#define C_BITS (CFG_BITS+CBG_BITS+CFL_BITS)
#define C_MASK (~(-1U<<C_BITS))
#define CFG_MAX (~(-1U<<CFG_BITS))
#define CFG_MASK CFG_MAX
#define CBG_AT CFG_BITS
#define CBG_MAX (~(-1U<<CBG_BITS))
#define CBG_MASK (CBG_MAX<<CBG_AT)
#define CFL_AT (CFG_BITS+CBG_BITS)
#define CFL_MAX (~(-1U<<CFL_BITS))
#define CFL_MASK (CFL_MAX<<CFL_AT)
#define CFL_BLINK (C_BLINK<<CFL_AT)
#define CFL_ITALIC (C_ITALIC<<CFL_AT)
#define CFL_UNDERLINE (C_UNDERLINE<<CFL_AT)
#define CFL_STRIKETHRU (C_STRIKETHRU<<CFL_AT)

enum
{
    MSG_ALIAS,
    MSG_ACTION,
    MSG_SUBSTITUTE,
    MSG_EVENT,
    MSG_HIGHLIGHT,
    MSG_VARIABLE,
    MSG_ROUTE,
    MSG_GOTO,
    MSG_BIND,
    MSG_SYSTEM,
    MSG_PATH,
    MSG_ERROR,
    MSG_HOOK,
    MSG_LOG,
    MSG_TICK,
    MSG_RATELIMIT,
    MAX_MESVAR
};
enum
{
    HOOK_OPEN,
    HOOK_CLOSE,
    HOOK_ZAP,
    HOOK_END,
    HOOK_SEND,
    HOOK_ACTIVATE,
    HOOK_DEACTIVATE,
    HOOK_TITLE,
    HOOK_TICK,
    HOOK_PRETICK,
    HOOK_LOGCLOSE,
    NHOOKS
};

/************************ includes *********************/
#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <stdint.h>
#ifdef __FreeBSD__
# include <sys/../iconv.h>
#else
# include <iconv.h>
#endif
#include <ctype.h>
#include <wctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <wchar.h>
#include <errno.h>
#include <strings.h>
#ifdef HAVE_ZLIB
# include <zlib.h>
#endif
#include <string.h>
#ifdef HAVE_GNUTLS
# include <gnutls/gnutls.h>
# include <gnutls/x509.h>
#endif
#ifdef HAVE_SIMD
# include <hs/hs.h>
#endif
#ifdef  __cplusplus
# define restrict __restrict__
# define _Static_assert static_assert
#endif
#include "malloc.h"
#include "unicode.h"
#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t n);
#endif
#include "kbtree.h"

typedef int64_t num_t;
typedef int64_t timens_t;
typedef struct trip *ptrip;

#define NANO 1000000000LL
#define INVALID_TIME ((int64_t)0x8000000000000000LL)

#define ARRAYSZ(x) (sizeof(x)/sizeof((x)[0]))

KBTREE_HEADER(str, char*, strcmp)
KBTREE_HEADER(trip, ptrip, tripcmp)

#define TYPE_ITER(kind, type, tree, ip) {kbitr_t itr; kbtree_t(kind) *itrtr = (tree); \
    for (kb_itr_first(kind, itrtr, &itr); kb_itr_valid(&itr); kb_itr_next(kind, itrtr, &itr)) \
    { const type ip = kb_itr_key(type, &itr);
#define STR_ITER(tree, ip) TYPE_ITER(str, char*, (tree), ip)
#define TRIP_ITER(tree, ip) TYPE_ITER(trip, ptrip, (tree), ip)
#define ENDITER }}

/************************ structures *********************/
struct trip
{
    char *left, *right, *pr;
};

struct hashentry
{
    char *left;
    char *right;
};

struct hashtable
{
    int size;               /* allocated size */
    int nent;               /* current number of entries */
    int nval;               /* current number of values (entries-deleted) */
    struct hashentry *tab;  /* entries table */
};

struct completenode
{
    struct completenode *next;
    char *strng;
};

struct eventnode
{
    struct eventnode *next;
    char *event, *label;
    timens_t time, period;
};

struct routenode
{
    struct routenode *next;
    int dest;
    char *path;
    num_t distance;
    char *cond;
};

struct pair
{
    const char *left;
    const char *right;
};

struct pairlist
{
    int size;
    struct pair pairs[];
};

typedef enum { CM_ISO8859_1, CM_UTF8, CM_ICONV, CM_ASCII } conv_mode_t;

struct charset_conv
{
    const char *name;
    conv_mode_t mode;
    int dir; /* -1=only in, 0=both, 1=only out */
    iconv_t i_in, i_out;
};

typedef enum { LOG_RAW, LOG_LF, LOG_TTYREC } logtype_t;

typedef enum { SES_NULL, SES_SOCKET, SES_PTY, SES_SELFPIPE } sestype_t;

typedef enum {MUDC_OFF, MUDC_ON, MUDC_ANSI, MUDC_NULL, MUDC_NULL_WARN} mudcolors_t;

struct session
{
    struct session *next;
    char *name;
    char *address;
    bool tickstatus;
    timens_t time0;      /* time of last tick (adjusted every tick) */
    timens_t time10;
    timens_t tick_size, pretick;
    bool snoopstatus;
    FILE *logfile, *debuglogfile;
    char *logname, *debuglogname;
    char *loginputprefix, *loginputsuffix;
    logtype_t logtype;
    bool ignore;
    kbtree_t(trip) *subs, *actions, *prompts, *highs;
    kbtree_t(str) *antisubs;
    struct hashtable *aliases, *myvars, *pathdirs, *binds, *ratelimits;
    struct pair path[MAX_PATH_LENGTH];
    struct routenode **routes;
    char **locations;
    struct eventnode *events;
    int num_locations;
    int path_begin, path_length;
    int socket, last_term_type;
    sestype_t sestype;
    bool naws, ga, gas;
    int server_echo; /* 0=not negotiated, 1=we shouldn't echo, 2=we can echo */
    bool more_coming;
    char last_line[BUFFER_SIZE], telnet_buf[BUFFER_SIZE];
    int telnet_buflen;
    bool verbose, blank, echo, speedwalk, togglesubs, presub, verbatim;
    char *partial_line_marker;
    bool mesvar[MAX_MESVAR];
    timens_t idle_since, server_idle_since;
    timens_t sessionstart;
    char *hooks[NHOOKS];
    int closing;
    int nagle; /* reused as write end of the pipe for selfpipe */
    bool halfcr_in, halfcr_log; /* \r at the end of a packet */
    int lastintitle;
    char *charset, *logcharset;
    struct charset_conv c_io, c_log;
    mudcolors_t mudcolors;
    char *MUDcolors[16];
#ifdef HAVE_ZLIB
    bool can_mccp, mccp_more;
    z_stream *mccp;
    char mccp_buf[INPUT_CHUNK];
#endif
#ifdef HAVE_GNUTLS
    gnutls_session_t ssl;
#endif
    timens_t line_time;
    unsigned long long linenum;
    bool drafted;
#ifdef HAVE_SIMD
    bool highs_dirty, act_dirty[2], subs_dirty, antisubs_dirty;
    hs_database_t *highs_hs, *subs_hs, *antisubs_hs, *acts_hs[2];
    const char **highs_cols;
    ptrip *subs_data, *acts_data[2];
    int subs_omni_first, subs_omni_last;
    uintptr_t *subs_markers;
#endif
};

typedef char pvars_t[10][BUFFER_SIZE];

#define logcs_is_special(x) ((x)==LOGCS_LOCAL || (x)==LOGCS_REMOTE)
#define logcs_name(x) (((x)==LOGCS_LOCAL)?"local":              \
                       ((x)==LOGCS_REMOTE)?"remote":            \
                        (x))
#define logcs_charset(x) (((x)==LOGCS_LOCAL)?user_charset_name: \
                          ((x)==LOGCS_REMOTE)?ses->charset:     \
                           (x))

/* Chinese rod numerals are _not_ digits for our purposes. */
static inline bool isadigit(WC x) { return x>='0' && x<='9'; }
/* Solaris is buggy for high-bit chars in UTF-8. */
static inline bool isaspace(WC x) { return x==' ' || x=='\t' || x=='\n' || x==12 || x=='\v'; }
#define iswadigit(x) isadigit(x)
static inline bool isw2width(WC x)
{
    return x>=0x1100  && (x<=0x11ff ||
           x==0x2329 || x==0x232a   ||
           x>=0x2e80) && x!=0x303f
                      && (x<=0xa4cf ||
           x>=0xac00) && (x<=0xd7ff ||
           x>=0xf900) && (x<=0xfaff ||
           x>=0xfe30) && (x<=0xfe6f ||
           x>=0xff00) && (x<=0xff60 ||
           x>=0xffe0) && (x<=0xffe6 ||
           x>=0x20000) && x<=0x3ffff;
}

static inline bool is7alpha(WC x) { return (x>='A'&&x<='Z') || (x>='a'&&x<='z'); }
static inline bool is7alnum(WC x) { return (x>='0'&&x<='9') || is7alpha(x); }
static inline char toalower(char x) { return (x>='A' && x<='Z') ? x+32 : x; }
#define EMPTY_CHAR 0xffff
#define VALID_TIN_CHARS "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
#define is7punct(x) strchr(VALID_TIN_CHARS, (x))
#define N(x) ((x)*DENOM)

#define assert(p) do if (!(p)){fprintf(stderr, "ASSERT FAILED in %s:%u : "#p "\n", __FILE__, __LINE__);abort();}while(0)
#define ZERO(x) bzero(&(x), sizeof(x))

static inline void stracpy(char *restrict dst, const char *restrict src, size_t sz)
{
    size_t len = strlen(src) + 1;
    assert(len <= sz);
    memcpy(dst, src, len);
}

#ifdef HAVE_SIMD
// Should be in globals.h but for the typedef...
extern hs_scratch_t *hs_scratch;
#endif

#include "commands.h"
