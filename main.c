/*********************************************************************/
/* file: main.c - main module - signal setup/shutdown etc            */
/*                             TINTIN++                              */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include "protos/action.h"
#include "protos/antisub.h"
#include "protos/bind.h"
#include "protos/colors.h"
#include "protos/files.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/highlight.h"
#include "protos/history.h"
#include "protos/hooks.h"
#include "protos/print.h"
#include "protos/math.h"
#include "protos/misc.h"
#include "protos/mudcolors.h"
#include "protos/net.h"
#include "protos/parse.h"
#include "protos/session.h"
#include "protos/slist.h"
#include "protos/substitute.h"
#include "protos/ticks.h"
#include "protos/tlist.h"
#include "protos/unicode.h"
#include "protos/user.h"
#include "protos/utils.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

typedef void (*sighandler_t)(int);

extern void end_command(const char *arg, struct session *ses);

static void echo_input(const char *txt);
static bool eofinput=false;
static char prev_command[BUFFER_SIZE];
static void tintin(void);
static void read_mud(struct session *ses);
static void do_one_line(char *line, int nl, struct session *ses);
static void snoop(const char *buffer, struct session *ses);
static void myquitsig(int);

static void tstphandler(int sig)
{
    user_pause();
    kill(getpid(), SIGSTOP);
}

static void sigchild(void)
{
    while (waitpid(-1, 0, WNOHANG)>0);
}

static void sigcont(void)
{
    user_resume();
}

static void sigsegv(void)
{
    user_done();
    fflush(0);
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
}

static void sigfpe(void)
{
    user_done();
    fflush(0);
    signal(SIGFPE, SIG_DFL);
    raise(SIGFPE);
}

static void sighup(void)
{
    fflush(0);
    signal(SIGHUP, SIG_DFL);
    raise(SIGHUP);
}

static void sigwinch(void)
{
    need_resize=true;
}


/************************/
/* the #suspend command */
/************************/
void suspend_command(const char *arg, struct session *ses)
{
    tstphandler(SIGTSTP);
}

static void setup_signals(void)
{
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags=SA_RESTART;

    if (signal(SIGTERM, (sighandler_t)myquitsig) == SIG_ERR)
        syserr("signal SIGTERM");
    if (signal(SIGQUIT, (sighandler_t)myquitsig) == SIG_ERR)
        syserr("signal SIGQUIT");
    if (signal(SIGINT, (sighandler_t)myquitsig) == SIG_ERR)
        syserr("signal SIGINT");
    if (signal(SIGHUP, (sighandler_t)sighup) == SIG_ERR)
        syserr("signal SIGHUP");
    act.sa_handler=(sighandler_t)tstphandler;
    if (sigaction(SIGTSTP, &act, 0))
        syserr("sigaction SIGTSTP");

    if (ui_own_output)
    {
        act.sa_handler=(sighandler_t)sigcont;
        if (sigaction(SIGCONT, &act, 0))
            syserr("sigaction SIGCONT");
        act.sa_handler=(sighandler_t)sigwinch;
        if (sigaction(SIGWINCH, &act, 0))
            syserr("sigaction SIGWINCH");
    }

    if (ui_own_output)
    {
        if (signal(SIGSEGV, (sighandler_t)sigsegv) == SIG_ERR)
            syserr("signal SIGSEGV");
        if (signal(SIGFPE, (sighandler_t)sigfpe) == SIG_ERR)
            syserr("signal SIGFPE");
    }

    act.sa_handler=(sighandler_t)sigchild;
    if (sigaction(SIGCHLD, &act, 0))
        syserr("sigaction SIGCHLD");
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        syserr("signal SIGPIPE");
}

/****************************************/
/* attempt to assure enough stack space */
/****************************************/
static void setup_ulimit(void)
{
    struct rlimit rlim;

    if (getrlimit(RLIMIT_STACK, &rlim))
        return;
    if ((unsigned int)rlim.rlim_cur>=STACK_LIMIT)
        return;
    if (rlim.rlim_cur==rlim.rlim_max)
        return;
    if ((unsigned int)rlim.rlim_max<STACK_LIMIT)
        rlim.rlim_cur=rlim.rlim_max;
    else
        rlim.rlim_cur=STACK_LIMIT;
    setrlimit(RLIMIT_STACK, &rlim);
}

static void opterror(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

static struct trip *options;

static void parse_options(int argc, char **argv)
{
    bool noargs=false;

    options=MALLOC(argc*sizeof(struct trip));
    struct trip *o = options;

    for (int arg=1;arg<argc;arg++)
    {
        if (*argv[arg]!='-' || noargs)
            o->left=" ", o++->right=argv[arg];
        else if (!strcmp(argv[arg], "--"))
            noargs=true;
        else if (!strcmp(argv[arg], "--version")) /* make autotest happy */
        {
            printf("KBtin version "VERSION"\n");
            exit(0);
        }
        else if (!strcmp(argv[arg], "-v"))
            o++->left="#verbose 1";
        else if (!strcmp(argv[arg], "-q"))
            o++->left="#verbose 0";
        else if (!strcmp(argv[arg], "-p"))
            user_setdriver(0);
        else if (!strcmp(argv[arg], "-i"))
            user_setdriver(1);
        else if (!strcmp(argv[arg], "-c"))
        {
            if (++arg==argc)
                opterror("Invalid option: bare -c");
            else
                o->left="c", o++->right=argv[arg];
        }
        else if (!strcmp(argv[arg], "-r"))
        {
            if (++arg==argc)
                opterror("Invalid option: bare -r");
            else
                o->left="r", o++->right=argv[arg];
        }
        else if (!strcasecmp(argv[arg], "-s"))
        {
            if (++arg==argc)
                opterror("Invalid option: bare %s", argv[arg]);
            else if (++arg==argc)
                opterror("Bad option: -s needs both an address and a port number!");
            else
                o->left=argv[arg-2]+1, o->right=argv[arg-1], o++->pr=argv[arg];
        }
        else if (!strcmp(argv[arg], "--no-simd"))
#ifdef HAVE_SIMD
            simd=false;
#else
            ;
#endif
        else
            opterror("Invalid option: {%s}", argv[arg]);
    }
    o->left=0;
}

static void apply_options(void)
{
    char temp[BUFFER_SIZE], sname[BUFFER_SIZE];
    char ustr[BUFFER_SIZE];
    FILE *f;
# define DO_INPUT(str,iv) local_to_utf8(ustr, str, BUFFER_SIZE, 0);\
                          activesession=parse_input(ustr, iv, activesession);

    for (struct trip *opt=options; opt->left; opt++)
    {
        switch (*opt->left)
        {
        case '#':
            snprintf(ustr, sizeof(ustr), "%c%s", tintin_char, opt->left+1);
            activesession=parse_input(opt->left, true, activesession);
            break;
        case 'c':
            DO_INPUT(opt->right, false);
            break;
        case 'r':
            set_magic_hook(activesession);
            make_name(sname, opt->right);
            snprintf(temp, BUFFER_SIZE,
                "%crun %.*s {%s}", tintin_char, MAX_SESNAME_LENGTH, sname, opt->right);
            DO_INPUT(temp, true);
            break;
        case 's':
            set_magic_hook(activesession);
            make_name(sname, opt->right);
            snprintf(temp, BUFFER_SIZE,
                "%cses %.*s {%s %s}", tintin_char, MAX_SESNAME_LENGTH, sname, opt->right, opt->pr);
            DO_INPUT(temp, true);
            break;
        case 'S':
            set_magic_hook(activesession);
            make_name(sname, opt->right);
            snprintf(temp, BUFFER_SIZE,
                "%csslses %.*s {%s %s}", tintin_char, MAX_SESNAME_LENGTH, sname, opt->right, opt->pr);
            DO_INPUT(temp, true);
            break;
        case ' ':
            local_to_utf8(ustr, opt->right, BUFFER_SIZE, 0);
            if ((f=fopen(opt->right, "r")))
            {
                if (activesession->verbose || !real_quiet)
                    tintin_printf(activesession, "#READING {%s}", ustr);
                activesession = do_read(f, ustr, activesession);
            }
            else
                tintin_eprintf(0, "#FILE NOT FOUND: {%s}", ustr);
            break;
        }
    }

    free(options);
}

static bool read_rc_file(const char *path)
{
    char temp[BUFFER_SIZE], ustr[BUFFER_SIZE];
    snprintf(temp, sizeof temp, "%s/.tintinrc", path);
    FILE *f = fopen(temp, "r");
    if (!f)
        return false;
    local_to_utf8(ustr, temp, BUFFER_SIZE, 0);
    activesession = do_read(f, ustr, activesession);
    return true;
}

static void read_rc(void)
{
    if (!strcmp(DEFAULT_FILE_DIR, "HOME") && read_rc_file(DEFAULT_FILE_DIR))
        return;
    const char *home = getenv("HOME");
    if (home)
        read_rc_file(home);
}

static void banner(void)
{
    /*
        Legal crap does _not_ belong here.  Anyone interested in the license
    can check the files accompanying KBtin, without any need of being spammed
    every time.  It is not GNU bc or something similar, we don't want half
    a screenful of all-uppercase (cAPS kEY IS STUCK AGAIN?) text that no one
    ever wants to read -- that is what docs are for.
    */
    tintin_printf(0, "~2~##########################################################");
    tintin_printf(0, "#~7~               ~12~K B ~3~t i n~7~    v %-25s ~2~#", VERSION);
    tintin_printf(0, "#                                                        #");
    tintin_printf(0, "#~7~ current developer: ~9~Adam Borowski (~9~kilobyte@angband.pl~7~) ~2~#");
    tintin_printf(0, "#                                                        #");
    tintin_printf(0, "#~7~ based on ~12~tintin++~7~ v 2.1.9 by Peter Unold, Bill Reiss,  ~2~#");
    tintin_printf(0, "#~7~   David A. Wagner, Joann Ellsworth, Jeremy C. Jack,    ~2~#");
    tintin_printf(0, "#~7~          Ulan@GrimneMUD and Jakub Narębski             ~2~#");
    tintin_printf(0, "##########################################################~7~");
    tintin_printf(0, "~15~#session <name> <host> <port> ~7~to connect to a remote server");
    tintin_printf(0, "                              ~8~#ses t2t t2tmud.org 9999");
    tintin_printf(0, "~15~#run <name> <command>         ~7~to run a local command");
    tintin_printf(0, "                              ~8~#run advent adventure");
    tintin_printf(0, "                              ~8~#run sql mysql");
    tintin_printf(0, "~15~#help                         ~7~to get the help index");
}

/**************************************************************************/
/* main() - show title - setup signals - init lists - readcoms - tintin() */
/**************************************************************************/
int main(int argc, char **argv)
{
    tintin_exec=argv[0];
    init_locale();
    user_setdriver(isatty(0)?1:0);
    parse_options(argc, argv);
#ifdef HAVE_SIMD
    if (simd && hs_valid_platform())
        simd=false;
#endif
    init_bind();
    hist_num=-1;
    init_parse();
    strcpy(status, EMPTY_LINE);
    user_init();
    /*  read_complete();            no tab-completion */
    srand((((unsigned)getpid())*0x10001)^start_time^(start_time>>32));
    lastdraft=0;

    if (ui_own_output || tty)
        banner();
    user_mark_greeting();

    setup_signals();
    setup_ulimit();
    init_nullses();
    read_rc();
    apply_options();
    tintin();
    return 0;
}

/******************************************************/
/* return seconds to next tick (global, all sessions) */
/* also display tick messages                         */
/******************************************************/
static timens_t check_events(void)
{
    timens_t tick_time = 0, curr_time, tt;

    curr_time = current_time();
restart:
    any_closed=false;
    for (struct session *sp = sessionlist; sp; sp = sp->next)
    {
        tt = check_event(curr_time, sp);
        if (any_closed)
            goto restart;
        if (tt > curr_time && (tick_time == 0 || tt < tick_time))
            tick_time = tt;
    }

    if (tick_time > curr_time)
        return tick_time - curr_time;
    return 0;
}

/***************************/
/* the main loop of tintin */
/***************************/
static void tintin(void)
{
    int i, result, maxfd;
    struct timeval tv;
    fd_set readfdmask;
#ifdef XTERM_TITLE
    struct session *lastsession=0;
#endif
    char kbdbuf[BUFFER_SIZE];
    WC ch;
    int inbuf=0;
    mbstate_t instate;

    ZERO(instate);

    for (;;)
    {
#ifdef XTERM_TITLE
        if (ui_own_output && activesession!=lastsession)
        {
            lastsession=activesession;
            if (activesession==nullsession)
                user_title(XTERM_TITLE, "(no session)");
            else
                user_title(XTERM_TITLE, activesession->name);
        }
#endif

        timens_t ev = check_events();
        tv.tv_sec = ev/NANO;
        tv.tv_usec = ev%NANO/1000;

        maxfd=0;
        FD_ZERO(&readfdmask);
        if (!eofinput)
            FD_SET(0, &readfdmask);
        else if (activesession==nullsession)
            end_command(0, activesession);
        for (struct session *ses = sessionlist; ses; ses = ses->next)
        {
            if (ses==nullsession)
                continue;
            if (ses->sestype==SES_SOCKET && ses->nagle)
                flush_socket(ses);
            FD_SET(ses->socket, &readfdmask);
            if (ses->socket>maxfd)
                maxfd=ses->socket;
        }
        result = select(maxfd+1, &readfdmask, 0, 0, &tv);

        if (need_resize)
        {
            char buf[BUFFER_SIZE];

            user_resize();
            sprintf(buf, "#NEW SCREEN SIZE: %dx%d.", COLS, LINES);
            tintin_puts1(buf, activesession);
        }

        if (result == 0)
            continue;
        else if (result < 0 && errno == EINTR)
            continue;   /* Interrupted system call */
        else if (result < 0)
            syserr("select");

        if (FD_ISSET(0, &readfdmask))
        {
            idle_since = current_time();
            result=read(0, kbdbuf+inbuf, BUFFER_SIZE-inbuf);
            if (result==-1)
                myquitsig(0);
            if (result==0 && !isatty(0))
                eofinput=true;
            inbuf+=result;

            i=0;
            while (i<inbuf)
            {
                result=mbrtowc(&ch, kbdbuf+i, inbuf-i, &instate);
                if (result==-2)         /* incomplete but valid sequence */
                {
                    memmove(kbdbuf, kbdbuf+i, inbuf-i);
                    inbuf-=i;
                    goto partial;
                }
                else if (result==-1)    /* invalid sequence */
                {
                    ch=0xFFFD;
                    i++;
                    errno=0;
                    /* Shift by 1 byte.  We can use a more intelligent shift,
                     * but staying charset-agnostic makes the code simpler.
                     */
                }
                else if (result==0)     /* literal 0 */
                    i++; /* oops... bad ISO/ANSI, bad */
                else
                    i+=result;
                if (user_process_kbd(activesession, ch))
                {
                    hist_num=-1;

                    if (term_echoing || (got_more_kludge && done_input[0]))
                        /* got_more_kludge: echo any non-empty line */
                    {
                        if (activesession && *done_input)
                            if (strcmp(done_input, prev_command))
                                do_history(done_input, activesession);
                        if (activesession->echo)
                            echo_input(done_input);
                        if (activesession->logfile)
                            write_logf(activesession, done_input,
                                activesession->loginputprefix, activesession->loginputsuffix);
                    }
                    if (*done_input)
                        strcpy(prev_command, done_input);
                    aborting=false;
                    activesession = parse_input(done_input, false, activesession);
                    recursion=0;
                }
            }
            inbuf=0;
        partial:;
        }
        for (struct session *ses = sessionlist; ses; ses = ses->next)
        {
            if (ses->socket && FD_ISSET(ses->socket, &readfdmask))
            {
                aborting=false;
                any_closed=false;
                do
                {
                    read_mud(ses);
                    if (any_closed)
                    {
                        any_closed=false;
                        goto after_read;
                        /* The remaining sessions will be done after select() */
                    }
#ifdef HAVE_ZLIB
                } while (ses->mccp_more);
#else
                } while (0);
#endif
            }
        }
    after_read:
        if (activesession->server_echo
            && (2-activesession->server_echo != gotpassword))
        {
            gotpassword= 2-activesession->server_echo;
            if (!gotpassword)
                got_more_kludge=false;
            user_passwd(gotpassword && !got_more_kludge);
            term_echoing=!gotpassword;
        }
    }
}


/*************************************************************/
/* read text from mud and test for actions/snoop/substitutes */
/*************************************************************/
static void read_mud(struct session *ses)
{
    char buffer[BUFFER_SIZE], linebuffer[BUFFER_SIZE], *cpsource, *cpdest;
    char temp[BUFFER_SIZE];
    int didget, count;

    if ((didget = read_buffer_mud(buffer, ses))==-1)
    {
        cleanup_session(ses);
        if (ses == activesession)
            activesession = newactive_session();
        return;
    }
    else if (!didget)
        return; /* possible if only telnet protocol data was received */
    if (ses->logfile)
    {
        if (ses->logtype)
        {
            count=0;

            if (ses->halfcr_log)
            {
                ses->halfcr_log=false;
                if (buffer[0]!='\n')
                    temp[count++]='\r';
            }

            for (int n = 0; n < didget; n++)
                if (buffer[n] != '\r')
                    temp[count++] = buffer[n];
                else if (n+1==didget)
                    ses->halfcr_log=true;
                else if (buffer[n+1]!='\n')
                    temp[count++]='\r';
            temp[count]=0;      /* didget<BUFFER_SIZE, so no overflow */
            write_log(ses, temp, count);
        }
        else
        {
            buffer[didget]=0;
            write_log(ses, buffer, didget);
        }
    }

    cpsource = buffer;
    cpdest = stpcpy(linebuffer, ses->last_line);

    if (ses->halfcr_in)
    {
        ses->halfcr_in=false;
        goto halfcr;
    }
    while (*cpsource)
    {       /*cut out each of the lines and process */
        if (*cpsource == '\n')
        {
            *cpdest = '\0';
            do_one_line(linebuffer, 1, ses);
            ses->lastintitle=0;
            ses->drafted=false;

            cpsource++;
            cpdest=linebuffer;
        }
        else if (*cpsource=='\r')
        {
            cpsource++;
        halfcr:
            if (*cpsource=='\n')
                continue;
            if (!*cpsource)
            {
                ses->halfcr_in=true;
                break;
            }
            *cpdest=0;
            if (cpdest!=linebuffer)
            {
                do_one_line(linebuffer, 0, ses);
                ses->lastintitle=0;
                ses->drafted=false; /* abandoned */
            }
            cpdest=linebuffer;
        }
        else
            *cpdest++ = *cpsource++;
    }
    if (cpdest-linebuffer>INPUT_CHUNK) /* let's split too long lines */
    {
        *cpdest=0;
        do_one_line(linebuffer, 1, ses);
        ses->lastintitle=0;
        ses->drafted=false;
        cpdest=linebuffer;
    }
    *cpdest = '\0';
    strcpy(ses->last_line, linebuffer);
    if (!ses->more_coming)
        if (cpdest!=linebuffer)
        {
            do_one_line(linebuffer, 0, ses);
            ses->drafted=true;
        }
}

/**********************************************************/
/* do all of the functions to one line of buffer          */
/**********************************************************/
static void do_one_line(char *text, int nl, struct session *ses)
{
    bool isnb;
    char line[BUFFER_SIZE];
    timens_t t=0;

    if (nl)
        t = current_time();
    if (!ses->drafted)
        ses->linenum++;
    convert(&ses->c_io, line, text, -1);
    switch (ses->server_echo)
    {
    case 0:
        if ((match(PROMPT_FOR_PW_TEXT, line)
            || match(PROMPT_FOR_PW_TEXT2, line))
           && !gotpassword)
        {
            gotpassword=1;
            user_passwd(true);
            term_echoing=false;
        }
        break;
    case 1:
        if (match(PROMPT_FOR_MORE_TEXT, line))
        {
            user_passwd(false);
            got_more_kludge=true;
        }
    }
    _=line;
    do_in_MUD_colors(line, false, ses);
    isnb=isnotblank(line, false);
    if (!ses->ignore && (nl||isnb))
        check_all_promptactions(line, ses);
    if (nl && !ses->presub && !ses->ignore)
        check_all_actions(line, ses);
    if (!ses->togglesubs && (nl||isnb) && !do_one_antisub(line, ses))
        do_all_sub(line, ses);
    if (nl && ses->presub && !ses->ignore)
        check_all_actions(line, ses);
    if (isnb&&!ses->togglesubs)
        do_all_high(line, ses);
    if (isnotblank(line, ses->blank))
    {
        if (ses==activesession)
        {
            if (nl)
            {
                if (!activesession->server_echo)
                {
                    user_passwd(false);
                    gotpassword=0;
                }
                strcat(line, "\n");
                user_textout_draft(0, 0);
                user_textout(line);
                lastdraft=0;
            }
            else
            {
                if (ui_drafts)
                {
                    isnb = ses->gas ? ses->ga : iscompleteprompt(line);
                    if (ses->partial_line_marker)
                        strcat(line, ses->partial_line_marker);
                    user_textout_draft(line, isnb);
                }
                lastdraft=ses;
            }
        }
        else if (ses->snoopstatus)
            snoop(line, ses);
    }
    _=0;

    if (nl)
    {
        t = current_time() - t;
        if (ses->line_time)
        {
            /* A dragged average: every new line counts for 1% of the value.
              The weight of any old line decays with half-life around 69. */
            ses->line_time=(ses->line_time*99+t)/100;
        }
        else
            ses->line_time=t;
    }
}

/**********************************************************/
/* snoop session ses - chop up lines and put'em in buffer */
/**********************************************************/
static void snoop(const char *buffer, struct session *ses)
{
    tintin_printf(0, "%s%% %s\n", ses->name, buffer);
}

static void echo_input(const char *txt)
{
    const char *cptr;
    char out[BUFFER_SIZE], *optr;
    static int c=7;

    if (ui_own_output)
    {
        user_textout("");
        c=lastcolor;
    }

    optr=out;
    if (ui_own_output)
        optr+=sprintf(optr, ECHO_COLOR);
    cptr=txt;
    while (*cptr)
    {
        if ((*optr++=*cptr++)=='~')
            optr+=sprintf(optr, "~:~");
        if (optr-out > BUFFER_SIZE-10)
            break;
    }
    if (ui_own_output)
        optr+=sprintf(optr, "~%d~\n", c);
    else
    {
        *optr++='\n';
        *optr=0;
    }
    user_textout(out);
}

/**********************************************************/
/* Here's where we go when we wanna quit TINTIN FAAAAAAST */
/**********************************************************/
static void myquitsig(int sig)
{
    struct session *t;
    int err=errno;

    for (struct session *ses = sessionlist; ses; ses = t)
    {
        t = ses->next;
        if (ses!=nullsession && !ses->closing)
        {
            ses->closing=1;
            do_hook(ses, HOOK_ZAP, 0, true);
            ses->closing=0;
            cleanup_session(ses);
        }
    }
    activesession = nullsession;
    do_hook(nullsession, HOOK_END, 0, true);
    activesession = NULL;

    if (ui_own_output)
    {
        user_textout("~7~\n");
        switch (sig)
        {
        case SIGTERM:
            user_textout("Terminated\n");
            break;
        case SIGQUIT:
            user_textout("Quit\n");
            break;
        case SIGINT:
        default:
            break;
        case 0:
            user_textout(strerror(err));
        }
        user_done();
    }
    else if (tty)
        user_textout("~7~\n");
    exit(0);
}
