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
#include "protos/llist.h"
#include "protos/print.h"
#include "protos/misc.h"
#include "protos/net.h"
#include "protos/parse.h"
#include "protos/prof.h"
#include "protos/session.h"
#include "protos/substitute.h"
#include "protos/ticks.h"
#include "protos/unicode.h"
#include "protos/user.h"
#include "protos/utils.h"
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>

#ifndef BADSIG
#define BADSIG (void (*)(int))-1
#endif

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
    if (ui_own_output)
        user_pause();
    kill(getpid(), SIGSTOP);
}

static void sigchild(void)
{
    while (waitpid(-1, 0, WNOHANG)>0);
}

static void sigcont(void)
{
    if (ui_own_output)
        user_resume();
}

static void sigsegv(void)
{
    if (ui_own_output)
        user_done();
    fflush(0);
/*  write(2, "Segmentation fault.\n", 20);*/
    signal(SIGSEGV, SIG_DFL);
    raise(SIGSEGV);
/*  exit(SIGSEGV);*/
}

static void sigfpe(void)
{
    if (ui_own_output)
        user_done();
    fflush(0);
/*  write(2, "Floating point exception.\n", 26);*/
    signal(SIGFPE, SIG_DFL);
    raise(SIGFPE);
/*  exit(SIGFPE);*/
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


static bool new_news(void)
{
    struct stat KBtin, news;

    if (stat(tintin_exec, &KBtin))
        return false;       /* can't stat the executable??? */
    if (stat(NEWS_FILE, &news))
#ifdef DATA_PATH
        if (stat(DATA_PATH "/" NEWS_FILE, &news))
#endif
            return false;       /* either no NEWS file, or can't stat it */
    return (news.st_ctime>=news.st_atime)||(KBtin.st_ctime+10>news.st_atime);
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

    if (signal(SIGTERM, (sighandler_t)myquitsig) == BADSIG)
        syserr("signal SIGTERM");
    if (signal(SIGQUIT, (sighandler_t)myquitsig) == BADSIG)
        syserr("signal SIGQUIT");
    if (signal(SIGINT, (sighandler_t)myquitsig) == BADSIG)
        syserr("signal SIGINT");
    if (signal(SIGHUP, (sighandler_t)sighup) == BADSIG)
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
        if (signal(SIGSEGV, (sighandler_t)sigsegv) == BADSIG)
            syserr("signal SIGSEGV");
        if (signal(SIGFPE, (sighandler_t)sigfpe) == BADSIG)
            syserr("signal SIGFPE");
    }

    act.sa_handler=(sighandler_t)sigchild;
    if (sigaction(SIGCHLD, &act, 0))
        syserr("sigaction SIGCHLD");
    if (signal(SIGPIPE, SIG_IGN) == BADSIG)
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

static void init_nullses(void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);
    time0 = tv.tv_sec;
    utime0 = tv.tv_usec;

    nullsession=TALLOC(struct session);
    nullsession->name=mystrdup("main");
    nullsession->address=0;
    nullsession->tickstatus = false;
    nullsession->tick_size = DEFAULT_TICK_SIZE;
    nullsession->pretick = DEFAULT_PRETICK;
    nullsession->time0 = 0;
    nullsession->snoopstatus = true;
    nullsession->logfile = 0;
    nullsession->logname = 0;
    nullsession->logtype = DEFAULT_LOGTYPE;
    nullsession->loginputprefix=mystrdup(LOG_INPUT_PREFIX);
    nullsession->loginputsuffix=mystrdup(LOG_INPUT_SUFFIX);
    nullsession->blank = DEFAULT_DISPLAY_BLANK;
    nullsession->echo = ui_sep_input?DEFAULT_ECHO_SEPINPUT
                                    :DEFAULT_ECHO_NOSEPINPUT;
    nullsession->speedwalk = DEFAULT_SPEEDWALK;
    nullsession->togglesubs = DEFAULT_TOGGLESUBS;
    nullsession->presub = DEFAULT_PRESUB;
    nullsession->verbatim = false;
    nullsession->ignore = DEFAULT_IGNORE;
    nullsession->partial_line_marker = mystrdup(DEFAULT_PARTIAL_LINE_MARKER);
    nullsession->aliases = init_hash();
    nullsession->actions = init_list();
    nullsession->prompts = init_list();
    nullsession->subs = init_list();
    nullsession->myvars = init_hash();
    nullsession->highs = init_list();
    nullsession->pathdirs = init_hash();
    nullsession->socket = 0;
    nullsession->issocket = false;
    nullsession->naws = false;
#ifdef HAVE_ZLIB
    nullsession->can_mccp = false;
    nullsession->mccp = 0;
    nullsession->mccp_more = false;
#endif
    nullsession->last_term_type=0;
    nullsession->server_echo = 0;
    nullsession->nagle = false;
    nullsession->antisubs = init_list();
    nullsession->binds = init_hash();
    nullsession->next = 0;
    nullsession->sessionstart=nullsession->idle_since=
        nullsession->server_idle_since=time(0);
    nullsession->debuglogfile=0;
    nullsession->debuglogname=0;
    for (int i=0;i<HISTORY_SIZE;i++)
        history[i]=0;
    for (int i=0;i<MAX_LOCATIONS;i++)
    {
        nullsession->routes[i]=0;
        nullsession->locations[i]=0;
    }
    for (int i=0;i<NHOOKS;i++)
        nullsession->hooks[i]=0;
    nullsession->path = init_list();
    nullsession->no_return = 0;
    nullsession->path_length = 0;
    nullsession->last_line[0] = 0;
    nullsession->events = NULL;
    nullsession->verbose=false;
    nullsession->closing=false;
    sessionlist = nullsession;
    activesession = nullsession;
    pvars=0;

    nullsession->mesvar[MSG_ALIAS] = DEFAULT_ALIAS_MESS;
    nullsession->mesvar[MSG_ACTION] = DEFAULT_ACTION_MESS;
    nullsession->mesvar[MSG_SUBSTITUTE] = DEFAULT_SUB_MESS;
    nullsession->mesvar[MSG_EVENT] = DEFAULT_EVENT_MESS;
    nullsession->mesvar[MSG_HIGHLIGHT] = DEFAULT_HIGHLIGHT_MESS;
    nullsession->mesvar[MSG_VARIABLE] = DEFAULT_VARIABLE_MESS;
    nullsession->mesvar[MSG_ROUTE] = DEFAULT_ROUTE_MESS;
    nullsession->mesvar[MSG_GOTO] = DEFAULT_GOTO_MESS;
    nullsession->mesvar[MSG_BIND] = DEFAULT_BIND_MESS;
    nullsession->mesvar[MSG_SYSTEM] = DEFAULT_SYSTEM_MESS;
    nullsession->mesvar[MSG_PATH]= DEFAULT_PATH_MESS;
    nullsession->mesvar[MSG_ERROR]= DEFAULT_ERROR_MESS;
    nullsession->mesvar[MSG_HOOK]= DEFAULT_HOOK_MESS;
    nullsession->mesvar[MSG_LOG]= DEFAULT_LOG_MESS;
    nullsession->charset=mystrdup(DEFAULT_CHARSET);
    nullsession->logcharset=logcs_is_special(DEFAULT_LOGCHARSET) ?
                              DEFAULT_LOGCHARSET : mystrdup(DEFAULT_LOGCHARSET);
    nullify_conv(&nullsession->c_io);
    nullify_conv(&nullsession->c_log);
    nullsession->line_time.tv_sec=0;
    nullsession->line_time.tv_usec=0;
#ifdef HAVE_GNUTLS
    nullsession->ssl=0;
#endif
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

static struct listnode *options;

static void parse_options(int argc, char **argv)
{
    bool noargs=false;

    options=init_list();

    for (int arg=1;arg<argc;arg++)
    {
        if (*argv[arg]=='-' && !noargs)
        {
            if (!strcmp(argv[arg], "--"))
                noargs=true;
            else if (!strcmp(argv[arg], "--version")) /* make autotest happy */
            {
                printf("KBtin version "VERSION"\n");
                exit(0);
            }
            else if (!strcmp(argv[arg], "-v"))
                addnode_list(options, "#verbose 1", 0, 0);
            else if (!strcmp(argv[arg], "-q"))
                addnode_list(options, "#verbose 0", 0, 0);
            else if (!strcmp(argv[arg], "-p"))
                user_setdriver(0);
            else if (!strcmp(argv[arg], "-i"))
                user_setdriver(1);
            else if (!strcmp(argv[arg], "-c"))
            {
                if (++arg==argc)
                    opterror("Invalid option: bare -c");
                else
                    addnode_list(options, "c", argv[arg], 0);
            }
            else if (!strcmp(argv[arg], "-r"))
            {
                if (++arg==argc)
                    opterror("Invalid option: bare -r");
                else
                    addnode_list(options, "r", argv[arg], 0);
            }
            else if (!strcasecmp(argv[arg], "-s"))
            {
                if (++arg==argc)
                    opterror("Invalid option: bare %s", argv[arg]);
                else if (++arg==argc)
                    opterror("Bad option: -s needs both an address and a port number!");
                else
                    addnode_list(options, argv[arg-2]+1, argv[arg-1], argv[arg]);
            }
            else
                opterror("Invalid option: {%s}", argv[arg]);
        }
        else
            addnode_list(options, " ", argv[arg], 0);
    }
    if (argc<=1)
        addnode_list(options, "-", 0, 0);
}

static void apply_options(void)
{
    char temp[BUFFER_SIZE], sname[BUFFER_SIZE];
    char ustr[BUFFER_SIZE];
    const char *home;
    FILE *f;
# define DO_INPUT(str,iv) local_to_utf8(ustr, str, BUFFER_SIZE, 0);\
                          activesession=parse_input(str, iv, activesession);

    for (struct listnode *opt=options->next; opt; opt=opt->next)
    {
        switch (*opt->left)
        {
        case '#':
            *opt->left=tintin_char;
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
                    tintin_printf(0, "#READING {%s}", ustr);
                activesession = do_read(f, ustr, activesession);
            }
            else
                tintin_eprintf(0, "#FILE NOT FOUND: {%s}", ustr);
            break;
        case '-':
            if (!strcmp(DEFAULT_FILE_DIR, "HOME"))
                if ((home = getenv("HOME")))
                    strcpy(temp, home);
                else
                    *temp = '\0';
            else
                strcpy(temp, DEFAULT_FILE_DIR);

            strcat(temp, "/.tintinrc");
            local_to_utf8(ustr, temp, BUFFER_SIZE, 0);
            if ((f=fopen(temp, "r")))
                activesession = do_read(f, ustr, activesession);
            else if ((home = getenv("HOME")))
            {
                strcpy(temp, home);
                strcat(temp, "/.tintinrc");
                local_to_utf8(ustr, temp, BUFFER_SIZE, 0);
                if ((f=fopen(temp, "r")))
                    activesession = do_read(f, ustr, activesession);
            }
        }
    }

    kill_list(options);
}

/**************************************************************************/
/* main() - show title - setup signals - init lists - readcoms - tintin() */
/**************************************************************************/
int main(int argc, char **argv)
{
    struct session *ses;

    tintin_exec=argv[0];
    init_locale();
    user_setdriver(isatty(0)?1:0);
    parse_options(argc, argv);
    init_bind();
    hist_num=-1;
    init_parse();
    strcpy(status, EMPTY_LINE);
    user_init();
    /*  read_complete();            no tab-completion */
    ses = NULL;
    srand((getpid()*0x10001)^time0);
    lastdraft=0;

    if (ui_own_output || tty)
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
        if (new_news())
            tintin_printf(ses, "Check #news now!");
    }
    user_mark_greeting();

    setup_signals();
#ifdef PROFILING
    setup_prof();
#endif
    PROF("initializing");
    setup_ulimit();
    init_nullses();
    PROF("other");
    apply_options();
    tintin();
    return 0;
}

/******************************************************/
/* return seconds to next tick (global, all sessions) */
/* also display tick messages                         */
/******************************************************/
static int check_events(void)
{
    int tick_time = 0, curr_time, tt;

    curr_time = time(NULL);
restart:
    any_closed=false;
    for (struct session *sp = sessionlist; sp; sp = sp->next)
    {
        tt = check_event(curr_time, sp);
        if (any_closed)
            goto restart;
        /* printf("#%s %d(%d)\n", sp->name, tt, curr_time); */
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

    memset(&instate, 0, sizeof(instate));

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

        tv.tv_sec = check_events();
        tv.tv_usec = 0;

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
            if (ses->nagle)
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
            PROFSTART;
            PROFPUSH("user interface");
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
        partial:
            PROFEND(kbd_lag, kbd_cnt);
            PROFPOP;
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

    PROFSTART;
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
    strcpy(linebuffer, ses->last_line);
    cpdest = strchr(linebuffer, '\0');

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
        cpdest=linebuffer;
    }
    *cpdest = '\0';
    strcpy(ses->last_line, linebuffer);
    if (!ses->more_coming)
        if (cpdest!=linebuffer)
            do_one_line(linebuffer, 0, ses);
    PROFEND(mud_lag, mud_cnt);
}

/**********************************************************/
/* do all of the functions to one line of buffer          */
/**********************************************************/
static void do_one_line(char *line, int nl, struct session *ses)
{
    bool isnb;
    char ubuf[BUFFER_SIZE];
    struct timeval t1, t2;

    if (nl)
        gettimeofday(&t1, 0);
    PROFPUSH("conv: remote->utf8");
    convert(&ses->c_io, ubuf, line, -1);
# define line ubuf
    PROF("looking for passwords");
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
    PROF("processing incoming colors");
    do_in_MUD_colors(line, false, ses);
    isnb=isnotblank(line, false);
    PROF("promptactions");
    if (!ses->ignore && (nl||isnb))
        check_all_promptactions(line, ses);
    PROF("actions");
    if (nl && !ses->presub && !ses->ignore)
        check_all_actions(line, ses);
    PROF("substitutions");
    if (!ses->togglesubs && (nl||isnb) && !do_one_antisub(line, ses))
        do_all_sub(line, ses);
    PROF("actions");
    if (nl && ses->presub && !ses->ignore)
        check_all_actions(line, ses);
    PROF("highlights");
    if (isnb&&!ses->togglesubs)
        do_all_high(line, ses);
    PROF("display");
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
                sprintf(strchr(line, 0), "\n");
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
                        sprintf(strchr(line, 0), "%s", ses->partial_line_marker);
                    user_textout_draft(line, isnb);
                }
                lastdraft=ses;
            }
        }
        else if (ses->snoopstatus)
            snoop(line, ses);
    }
    PROFPOP;
    _=0;
#undef line

    if (nl)
    {
        gettimeofday(&t2, 0);
        t2.tv_sec-=t1.tv_sec;
        t2.tv_usec-=t1.tv_usec;
        if (t2.tv_usec<0)
            t2.tv_usec+=1000000, t2.tv_sec--;
        if (ses->line_time.tv_sec || ses->line_time.tv_usec)
        {
            /* A dragged average: every new line counts for 10% of the value.
              The weight of any old line decays with half-life around 6.5. */
            int64_t t=(int64_t)(ses->line_time.tv_sec*9+t2.tv_sec)*100000
                              +(ses->line_time.tv_usec*9+t2.tv_usec)/10;
            ses->line_time.tv_sec =t/1000000;
            ses->line_time.tv_usec=t%1000000;
        }
        else
            ses->line_time=t2;
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
