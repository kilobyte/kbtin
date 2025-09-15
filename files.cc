/**********************************************************************/
/* file: files.c - functions for logfile and reading/writing comfiles */
/*                             TINTIN + +                             */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t              */
/*                     coded by peter unold 1992                      */
/*                    New code by Bill Reiss 1993                     */
/**********************************************************************/
#include "tintin.h"
#include "protos/action.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/hooks.h"
#include "protos/print.h"
#include "protos/math.h"
#include "protos/net.h"
#include "protos/parse.h"
#include "protos/run.h"
#include "protos/unicode.h"
#include "protos/user.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/vars.h"
#include "ttyrec.h"
#include <pwd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/time.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif

static void cfcom(FILE *f, const char *command, const char *left, const char *right, const char *pr);
extern void char_command(const char *arg, session *ses);

/*******************************/
/* expand tildes in a filename */
/********************************************/
/* result must be at least BUFFER_SIZE long */
/********************************************/
static void expand_filename(const char *arg, char *result, char *lstr)
{
    char *r0=result;

    if (*arg=='~')
    {
        if (*(arg+1)=='/')
            result+=snprintf(result, BUFFER_SIZE, "%s", getenv("HOME")), arg++;
        else
        {
            const char *p=strchr(arg+1, '/');
            if (p)
            {
                char name[BUFFER_SIZE];
                memcpy(name, arg+1, p-arg-1);
                name[p-arg-1]=0;
                struct passwd *pwd=getpwnam(name);
                if (pwd)
                    result+=snprintf(result, BUFFER_SIZE, "%s", pwd->pw_dir), arg=p;
            }
        }
    }
    strlcpy(result, arg, r0-result+BUFFER_SIZE);
    utf8_to_local(lstr, r0);
}

/****************************************/
/* convert charsets and write to a file */
/****************************************/
static void cfputs(const char *s, FILE *f)
{
    char lstr[BUFFER_SIZE*8];

    utf8_to_local(lstr, s);
    fputs(lstr, f);
}

static void cfprintf(FILE *f, const char *fmt, ...)
{
    char lstr[BUFFER_SIZE*8], buf[BUFFER_SIZE*4];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, BUFFER_SIZE*4, fmt, ap);
    va_end(ap);

    utf8_to_local(lstr, buf);
    fputs(lstr, f);
}

#if 0
/**********************************/
/* load a completion file         */
/**********************************/
void read_complete(const char *arg, session *ses)
{
    FILE *myfile;
    char buffer[BUFFER_SIZE];
    bool flag = true;
    struct completenode *tcomplete, *tcomp2;

    if (!(complete_head = (struct completenode *)(malloc(sizeof(struct completenode)))))
    {
        fprintf(stderr, "couldn't alloc completehead\n");
        user_done();
        exit(1);
    }
    tcomplete = complete_head;

    if (!(myfile = fopen("tab.txt", "r")))
    {
        if (const char *cptr = getenv("HOME"))
        {
            snprintf(buffer, BUFFER_SIZE, "%s/.tab.txt", cptr);
            myfile = fopen(buffer, "r");
        }
    }
    if (!myfile)
    {
        tintin_eprintf(0, "no tab.txt file, no completion list");
        return;
    }
    while (fgets(buffer, sizeof(buffer), myfile))
    {
        char *cptr;
        for (cptr = buffer; *cptr && *cptr != '\n'; cptr++) ;
        *cptr = '\0';
        if (!(tcomp2 = (struct completenode *)(malloc(sizeof(struct completenode)))))
        {
            fprintf(stderr, "couldn't alloc completehead\n");
            user_done();
            exit(1);
        }
        if (!(cptr = (char *)(malloc(strlen(buffer) + 1))))
        {
            fprintf(stderr, "couldn't alloc memory for string in complete\n");
            user_done();
            exit(1);
        }
        strcpy(cptr, buffer);
        tcomp2->strng = cptr;
        tcomplete->next = tcomp2;
        tcomplete = tcomp2;
    }
    tcomplete->next = NULL;
    fclose(myfile);
    tintin_printf(0, "tab.txt file loaded.");
    tintin_printf(0, "");

}
#endif

/*******************************/
/* remove file from filesystem */
/*******************************/
void unlink_command(const char *arg, session *ses)
{
    char file[BUFFER_SIZE], temp[BUFFER_SIZE], lstr[BUFFER_SIZE];

    if (!*arg)
        return tintin_eprintf(ses, "#ERROR: valid syntax is: #unlink <filename>");

    arg = get_arg_in_braces(arg, temp, 1);
    substitute_vars(temp, temp, ses);
    expand_filename(temp, file, lstr);
    unlink(lstr);
}

/*************************/
/* the #deathlog command */
/*************************/
void deathlog_command(const char *arg, session *ses)
{
    FILE *fh;
    char fname[BUFFER_SIZE], text[BUFFER_SIZE], temp[BUFFER_SIZE], lfname[BUFFER_SIZE];

    if (!*arg)
        return tintin_eprintf(ses, "#ERROR: valid syntax is: #deathlog <file> <text>");

    arg = get_arg_in_braces(arg, temp, 0);
    arg = get_arg_in_braces(arg, text, 1);
    substitute_vars(temp, temp, ses);
    expand_filename(temp, fname, lfname);
    substitute_vars(text, text, ses);
    if (!(fh = fopen(lfname, "a")))
        return tintin_eprintf(ses, "#ERROR: COULDN'T OPEN FILE {%s}.", fname);

    cfprintf(fh, "%s\n", text);
    fclose(fh);
}

void log_off(session *ses)
{
    fclose(ses->logfile);
    ses->logfile = NULL;
    char *logname = ses->logname;
    ses->logname = NULL;
    cleanup_conv(&ses->c_log);

    do_hook(ses, HOOK_LOGCLOSE, logname, false);

    SFREE(logname);
}

static inline void ttyrec_timestamp(struct ttyrec_header *th)
{
    struct timeval t;

    gettimeofday(&t, 0);
    th->sec=to_little_endian(t.tv_sec);
    th->usec=to_little_endian(t.tv_usec);
}

/* charset is always UTF-8 */
void write_logf(session *ses, const char *txt, const char *prefix, const char *suffix)
{
    char buf[BUFFER_SIZE*2], lbuf[BUFFER_SIZE*2];
    int len;

    sprintf(buf, "%s%s%s%s\n", prefix, txt, suffix, ses->logtype?"":"\r");
    if (ses->logtype==LOG_TTYREC)
    {
        ttyrec_timestamp((struct ttyrec_header *)lbuf);
        len=sizeof(struct ttyrec_header);
    }
    else
        len=0;
    convert(&ses->c_log, lbuf+len, buf, 1);
    len+=strlen(lbuf+len);
    if (ses->logtype==LOG_TTYREC)
    {
        uint32_t blen=len-sizeof(struct ttyrec_header);
        lbuf[ 8]=blen;
        lbuf[ 9]=blen>>8;
        lbuf[10]=blen>>16;
        lbuf[11]=blen>>24;
    }

    if ((int)fwrite(lbuf, 1, len, ses->logfile)<len)
    {
        log_off(ses);
        tintin_eprintf(ses, "#WRITE ERROR -- LOGGING DISABLED.  Disk full?");
    }
}

/* charset is always {remote} */
void write_log(session *ses, const char *txt, int n)
{
    struct ttyrec_header th;
    char ubuf[BUFFER_SIZE*2], lbuf[BUFFER_SIZE*2];

    if (ses->logcharset!=LOGCS_REMOTE && strcasecmp(user_charset_name, ses->charset))
    {
        convert(&ses->c_io, ubuf, txt, -1);
        convert(&ses->c_log, lbuf, ubuf, 1);
        n=strlen(lbuf);
        txt=lbuf;
    }
    if (ses->logtype==LOG_TTYREC)
    {
        ttyrec_timestamp(&th);
        th.len=to_little_endian(n);
        if (fwrite(&th, 1, sizeof(struct ttyrec_header), ses->logfile)<
            sizeof(struct ttyrec_header))
        {
            log_off(ses);
            tintin_eprintf(ses, "#WRITE ERROR -- LOGGING DISABLED.  Disk full?");
            return;
        }
    }

    if ((int)fwrite(txt, 1, n, ses->logfile)<n)
    {
        log_off(ses);
        tintin_eprintf(ses, "#WRITE ERROR -- LOGGING DISABLED.  Disk full?");
    }
}

/***************************/
/* the #logcomment command */
/***************************/
void logcomment_command(const char *arg, session *ses)
{
    char text[BUFFER_SIZE];

    if (!*arg)
        return tintin_eprintf(ses, "#Logcomment what?");

    if (!ses->logfile)
        return tintin_eprintf(ses, "#You're not logging.");

    arg=get_arg(arg, text, 1, ses);
    write_logf(ses, text, "", "");
}

/*******************************/
/* the #loginputformat command */
/*******************************/
void loginputformat_command(const char *arg, session *ses)
{
    char text[BUFFER_SIZE];

    arg=get_arg(arg, text, 0, ses);
    SFREE(ses->loginputprefix);
    ses->loginputprefix = mystrdup(text);
    arg=get_arg(arg, text, 1, ses);
    SFREE(ses->loginputsuffix);
    ses->loginputsuffix = mystrdup(text);
    tintin_printf(MSG_LOG, ses, "#OK. INPUT LOG FORMAT NOW: %s%%0%s",
        ses->loginputprefix, ses->loginputsuffix);
}

static bool lock_file(int fd, session *ses, const char *fname)
{
    if (!flock(fd, LOCK_EX|LOCK_NB))
        return false;
    if (errno==EWOULDBLOCK)
        tintin_eprintf(ses, "ERROR: LOG FILE {%s} ALREADY IN USE", fname);
    else
        tintin_eprintf(ses, "ERROR: COULDN'T LOCK FILE {%s}: %s", fname,
            strerror(errno));
    return true;
}

static FILE* open_logfile(session *ses, const char *name, const char *filemsg, const char *appendmsg, const char *pipemsg)
{
    char fname[BUFFER_SIZE], lfname[BUFFER_SIZE];
    FILE *f;

    if (*name=='|')
    {
        if (!*++name)
        {
            tintin_eprintf(ses, "#ERROR: {|} IS NOT A VALID PIPE.");
            return 0;
        }
        if ((f=mypopen(name, true, -1)))
            tintin_printf(MSG_LOG, ses, pipemsg, name);
        else
            tintin_eprintf(ses, "#ERROR: COULDN'T OPEN PIPE: {%s}.", name);
        return f;
    }
    if (*name=='>')
    {
        if (*++name=='>')
        {
            expand_filename(++name, fname, lfname);
            if ((f=fopen(lfname, "a")))
            {
                if (lock_file(fileno(f), ses, fname))
                    return fclose(f), nullptr;
                tintin_printf(MSG_LOG, ses, appendmsg, fname);
            }
            else
                tintin_eprintf(ses, "ERROR: COULDN'T APPEND TO FILE: {%s}.", fname);
            return f;
        }
        expand_filename(name, fname, lfname);
        if ((f=fopen(lfname, "w")))
        {
            if (lock_file(fileno(f), ses, fname))
                return fclose(f), nullptr;
            tintin_printf(MSG_LOG, ses, filemsg, fname);
        }
        else
            tintin_eprintf(ses, "ERROR: COULDN'T OPEN FILE: {%s}.", fname);
        return f;
    }
    expand_filename(name, fname, lfname);
    int len=strlen(fname);
    const char *zip=0;
    if (len>=4 && !strcmp(fname+len-3, ".gz"))
        zip="gzip -9";
    else if (len>=5 && !strcmp(fname+len-4, ".bz2"))
        zip="bzip2";
    else if (len>=5 && !strcmp(fname+len-4, ".bz3"))
        zip="bzip3";
    else if (len>=4 && !strcmp(fname+len-3, ".xz"))
        zip="xz";
    else if (len>=5 && !strcmp(fname+len-4, ".zst"))
        zip="zstd";
    if (zip)
    {
        int fd=open(lfname, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0666);
        if (fd==-1)
        {
            tintin_eprintf(ses, "#ERROR: COULDN'T CREATE FILE {%s}: %s",
                           fname, strerror(errno));
            return 0;
        }

        if (lock_file(fd, ses, fname))
            return close(fd), nullptr;

        if ((f = mypopen(zip, true, fd)))
            tintin_printf(MSG_LOG, ses, filemsg, fname);
        else
            tintin_eprintf(ses, "#ERROR: COULDN'T OPEN PIPE {%s >%s}.", zip, fname);
    }
    else if ((f = fopen(lfname, "w")))
        {
            if (lock_file(fileno(f), ses, fname))
                return fclose(f), nullptr;
            tintin_printf(MSG_LOG, ses, filemsg, fname);
        }
        else
            tintin_eprintf(ses, "#ERROR: COULDN'T OPEN FILE {%s}.", fname);
    return f;
}

/************************/
/* the #condump command */
/************************/
void condump_command(const char *arg, session *ses)
{
    char temp[BUFFER_SIZE];

    if (!ui_con_buffer)
        return tintin_eprintf(ses, "#UI: No console buffer => can't dump it.");

    if (!*arg)
        return tintin_eprintf(ses, "#Syntax: #condump <file>");

    arg = get_arg_in_braces(arg, temp, 0);
    substitute_vars(temp, temp, ses);
    FILE *fh=open_logfile(ses, temp,
        "#DUMPING CONSOLE TO {%s}",
        "#APPENDING CONSOLE DUMP TO {%s}",
        "#PIPING CONSOLE DUMP TO {%s}");
    if (fh)
    {
        user_condump(fh);
        fclose(fh);
    }
}

/********************/
/* the #log command */
/********************/
void log_command(const char *arg, session *ses)
{
    if (ses==nullsession)
        return tintin_eprintf(ses, "#THERE'S NO SESSION TO LOG.");

    if (*arg)
    {
        if (ses->closing)
            return tintin_eprintf(ses, "THE SESSION IS BEING CLOSED.");

        if (ses->logfile)
        {
            log_off(ses);
            tintin_printf(MSG_LOG, ses, "#OK. LOGGING TURNED OFF.");
        }

        if (ses->logfile)
            return; // tricksy buggers can #log in hook logclose

        char temp[BUFFER_SIZE];
        get_arg_in_braces(arg, temp, 1);
        substitute_vars(temp, temp, ses);
        ses->logfile=open_logfile(ses, temp,
            "#OK. LOGGING TO {%s} .....",
            "#OK. APPENDING LOG TO {%s} .....",
            "#OK. PIPING LOG TO {%s} .....");
        if (ses->logfile)
            ses->logname=mystrdup(temp);
        if (!new_conv(&ses->c_log, logcs_charset(ses->logcharset), 1))
            tintin_eprintf(ses, "#Warning: can't open charset: %s",
                                logcs_charset(ses->logcharset));
    }
    else if (ses->logfile)
    {
        log_off(ses);
        tintin_printf(MSG_LOG, ses, "#OK. LOGGING TURNED OFF.");
    }
    else
        tintin_printf(MSG_LOG, ses, "#LOGGING ALREADY OFF.");
}

/*************************/
/* the #debuglog command */
/*************************/
void debuglog_command(const char *arg, session *ses)
{
    if (*arg)
    {
        if (ses->debuglogfile)
        {
            fclose(ses->debuglogfile);
            tintin_printf(MSG_LOG, ses, "#OK. DEBUGLOG TURNED OFF.");
            ses->debuglogfile = NULL;
            SFREE(ses->debuglogname);
            ses->debuglogname = NULL;
        }
        char temp[BUFFER_SIZE];
        get_arg_in_braces(arg, temp, 1);
        substitute_vars(temp, temp, ses);
        ses->debuglogfile=open_logfile(ses, temp,
            "#OK. DEBUGLOG SET TO {%s} .....",
            "#OK. DEBUGLOG APPENDING TO {%s} .....",
            "#OK. DEBUGLOG PIPED TO {%s} .....");
        if (ses->debuglogfile)
            ses->debuglogname=mystrdup(temp);
    }
    else if (ses->debuglogfile)
    {
        fclose(ses->debuglogfile);
        ses->debuglogfile = NULL;
        SFREE(ses->debuglogname);
        ses->debuglogname = NULL;
        tintin_printf(MSG_LOG, ses, "#OK. DEBUGLOG TURNED OFF.");
    }
    else
        tintin_printf(ses, "#DEBUGLOG ALREADY OFF.");
}

void debuglog(session *ses, const char *format, ...)
{
    va_list ap;
    char buf[BUFFER_SIZE];

    if (!ses->debuglogfile)
        return;

    timens_t t = current_time()-ses->sessionstart;
    va_start(ap, format);
    if (vsnprintf(buf, BUFFER_SIZE-1, format, ap)>BUFFER_SIZE-2)
        buf[BUFFER_SIZE-3]='>';
    va_end(ap);
    cfprintf(ses->debuglogfile, "%4lld.%06lld: %s\n", t/NANO, t%NANO/1000, buf);
}


session* do_read(FILE *myfile, const char *filename, session *ses)
{
    char line[BUFFER_SIZE], buffer[BUFFER_SIZE], lstr[BUFFER_SIZE], *cptr, *eptr;
    bool want_tt_char, ignore_lines;
    int nl;
    mbstate_t cs;

    ZERO(cs);

    want_tt_char = !in_read && !tintin_char_set;
    if (!ses->verbose)
        puts_echoing = false;
    if (!in_read)
    {
        alnum = 0;
        acnum = 0;
        subnum = 0;
        varnum = 0;
        hinum = 0;
        antisubnum = 0;
        routnum=0;
        bindnum=0;
        pdnum=0;
        hooknum=0;
    }
    in_read++;
    *buffer=0;
    ignore_lines=false;
    nl=0;
    while (fgets(lstr, BUFFER_SIZE, myfile))
    {
        local_to_utf8(line, lstr, BUFFER_SIZE, &cs);
        if (!nl++)
            if (line[0]=='#' && line[1]=='!') /* Unix hashbang script */
                continue;
        if (want_tt_char)
        {
            puts_echoing = ses->verbose || !real_quiet;
            if (is7punct(*line))
                char_command(line, ses);
            if (!ses->verbose)
                puts_echoing = false;
            want_tt_char = false;
        }
        for (cptr = line; *cptr && *cptr != '\n' && *cptr!='\r'; cptr++) ;
        *cptr = '\0';

        if (isaspace(*line) && *buffer && (*buffer==tintin_char))
        {
            cptr=(char*)space_out(line);
            if (ignore_lines || (strlen(cptr)+strlen(buffer) >= BUFFER_SIZE/2))
            {
                puts_echoing=true;
                tintin_eprintf(ses, "#ERROR! LINE %d TOO LONG IN %s, TRUNCATING", nl, filename);
                *line=0;
                ignore_lines=true;
                puts_echoing=ses->verbose;
            }
            else
            {
                if (*cptr &&
                    !isspace(*(eptr=strchr(buffer, 0)-1)) &&
                    (*eptr!=';') && (*cptr!=BRACE_CLOSE) &&
                    (*cptr!=tintin_char || *eptr!=BRACE_OPEN))
                    strcat(buffer, " ");
                strcat(buffer, cptr);
                continue;
            }
        }
        else
            ignore_lines=false;
        ses = parse_input(buffer, true, ses);
        recursion=0;
        strcpy(buffer, line);
    }
    if (*buffer)
    {
        ses=parse_input(buffer, true, ses);
        recursion=0;
    }
    in_read--;
    if (!in_read)
        puts_echoing = true;
    if (!ses->verbose && !in_read && !real_quiet)
    {
        if (alnum > 0)
            tintin_printf(ses, "#OK. %d ALIASES LOADED.", alnum);
        if (acnum > 0)
            tintin_printf(ses, "#OK. %d ACTIONS LOADED.", acnum);
        if (antisubnum > 0)
            tintin_printf(ses, "#OK. %d ANTISUBS LOADED.", antisubnum);
        if (subnum > 0)
            tintin_printf(ses, "#OK. %d SUBSTITUTES LOADED.", subnum);
        if (varnum > 0)
            tintin_printf(ses, "#OK. %d VARIABLES LOADED.", varnum);
        if (hinum > 0)
            tintin_printf(ses, "#OK. %d HIGHLIGHTS LOADED.", hinum);
        if (routnum > 0)
            tintin_printf(ses, "#OK. %d ROUTES LOADED.", routnum);
        if (bindnum > 0)
            tintin_printf(ses, "#OK. %d BINDS LOADED.", bindnum);
        if (pdnum > 0)
            tintin_printf(ses, "#OK. %d PATHDIRS LOADED.", pdnum);
        if (hooknum > 0)
            tintin_printf(ses, "#OK. %d HOOKS LOADED.", hooknum);
    }
    fclose(myfile);
    return ses;
}


/*********************/
/* the #read command */
/*********************/
session* read_command(const char *filename, session *ses)
{
    FILE *myfile;
    char buffer[BUFFER_SIZE], fname[BUFFER_SIZE], lfname[BUFFER_SIZE];

    get_arg_in_braces(filename, buffer, 1);
    substitute_vars(buffer, buffer, ses);
    expand_filename(buffer, fname, lfname);
    if (!*filename)
    {
        tintin_eprintf(ses, "#Syntax: #read filename");
        return ses;
    }
    if (!(myfile = fopen(lfname, "r")))
    {
        tintin_eprintf(ses, "#ERROR - COULDN'T OPEN FILE {%s}.", fname);
        return ses;
    }

    return do_read(myfile, fname, ses);
}


#define WFLAG(name, var, org)   if (var!=(org))                         \
                                {                                       \
                                    sprintf(num, "%d", var);            \
                                    cfcom(myfile, name, num, 0, 0);     \
                                }
#define SFLAG(name, var, org)   WFLAG(name, ses->var, org)
/**********************/
/* the #write command */
/**********************/
void write_command(const char *filename, session *ses)
{
    FILE *myfile;
    char buffer[BUFFER_SIZE*4], num[32], fname[BUFFER_SIZE], lfname[BUFFER_SIZE];
    struct pairlist *hl;
    struct pair *end;

    get_arg_in_braces(filename, buffer, 1);
    substitute_vars(buffer, buffer, ses);
    expand_filename(buffer, fname, lfname);
    if (!*filename)
        return tintin_eprintf(ses, "#ERROR: syntax is: #write <filename>");
    if (!(myfile = fopen(lfname, "w")))
        return tintin_eprintf(ses, "#ERROR - COULDN'T OPEN FILE {%s}.", fname);

    WFLAG("keypad", keypad, DEFAULT_KEYPAD);
    WFLAG("retain", retain, DEFAULT_RETAIN);
    SFLAG("echo", echo, ui_sep_input?DEFAULT_ECHO_SEPINPUT
                                    :DEFAULT_ECHO_NOSEPINPUT);
    SFLAG("ignore", ignore, DEFAULT_IGNORE);
    SFLAG("speedwalk", speedwalk, DEFAULT_SPEEDWALK);
    SFLAG("presub", presub, DEFAULT_PRESUB);
    SFLAG("togglesubs", togglesubs, DEFAULT_TOGGLESUBS);
    SFLAG("verbose", verbose, false);
    SFLAG("blank", blank, DEFAULT_DISPLAY_BLANK);
    SFLAG("messages aliases", mesvar[MSG_ALIAS], DEFAULT_ALIAS_MESS);
    SFLAG("messages actions", mesvar[MSG_ACTION], DEFAULT_ACTION_MESS);
    SFLAG("messages substitutes", mesvar[MSG_SUBSTITUTE], DEFAULT_SUB_MESS);
    SFLAG("messages events", mesvar[MSG_EVENT], DEFAULT_EVENT_MESS);
    SFLAG("messages highlights", mesvar[MSG_HIGHLIGHT], DEFAULT_HIGHLIGHT_MESS);
    SFLAG("messages variables", mesvar[MSG_VARIABLE], DEFAULT_VARIABLE_MESS);
    SFLAG("messages routes", mesvar[MSG_ROUTE], DEFAULT_ROUTE_MESS);
    SFLAG("messages gotos", mesvar[MSG_GOTO], DEFAULT_GOTO_MESS);
    SFLAG("messages binds", mesvar[MSG_BIND], DEFAULT_BIND_MESS);
    SFLAG("messages #system", mesvar[MSG_SYSTEM], DEFAULT_SYSTEM_MESS);
    SFLAG("messages paths", mesvar[MSG_PATH], DEFAULT_PATH_MESS);
    SFLAG("messages errors", mesvar[MSG_ERROR], DEFAULT_ERROR_MESS);
    SFLAG("messages hooks", mesvar[MSG_HOOK], DEFAULT_HOOK_MESS);
    SFLAG("messages log", mesvar[MSG_LOG], DEFAULT_LOG_MESS);
    SFLAG("messages ticks", mesvar[MSG_TICK], DEFAULT_TICK_MESS);
    SFLAG("verbatim", verbatim, false);
    if (ses->tick_size != DEFAULT_TICK_SIZE*NANO)
    {
        usecstr(num, ses->tick_size);
        cfprintf(myfile, "%cticksize %s\n", tintin_char, num);
    }
    if (ses->pretick != DEFAULT_PRETICK*NANO)
    {
        usecstr(num, ses->pretick);
        cfprintf(myfile, "%cpretick %s\n", tintin_char, num);
    }
    if (strcmp(DEFAULT_CHARSET, ses->charset))
        cfprintf(myfile, "%ccharset {%s}\n", tintin_char, ses->charset);
    if (strcmp(logcs_name(DEFAULT_LOGCHARSET), logcs_name(ses->logcharset)))
        cfprintf(myfile, "%clogcharset {%s}\n", tintin_char, logcs_name(ses->logcharset));

    hl = hash2list(ses->aliases, 0);
    end = &hl->pairs[0] + hl->size;
    for (struct pair *n = &hl->pairs[0]; n<end; n++)
        cfcom(myfile, "alias", n->left, n->right, 0);
    delete[] hl;

    TRIP_ITER(ses->actions, n)
        cfcom(myfile, "action", n->left, n->right, n->pr);
    ENDITER

    TRIP_ITER(ses->prompts, n)
        cfcom(myfile, "promptaction", n->left, n->right, n->pr);
    ENDITER

    for (const auto s : ses->antisubs)
        cfcom(myfile, "antisub", s, 0, 0);

    for (const auto& i: ses->subs)
    {
        const auto& n = i;
        if (strcmp(n.second, EMPTY_LINE))
            cfcom(myfile, "sub", n.first, n.second, 0);
        else
            cfcom(myfile, "gag", n.first, 0, 0);
    }

    hl = hash2list(ses->myvars, 0);
    end = &hl->pairs[0] + hl->size;
    for (struct pair *n = &hl->pairs[0]; n<end; n++)
        cfcom(myfile, "var", n->left, n->right, 0);
    delete[] hl;

    for (const auto& i: ses->highs)
        cfcom(myfile, "highlight", i.first, i.second, 0);

    hl = hash2list(ses->pathdirs, 0);
    end = &hl->pairs[0] + hl->size;
    for (struct pair *n = &hl->pairs[0]; n<end; n++)
        cfcom(myfile, "pathdir", n->left, n->right, 0);
    delete[] hl;

    int n = ses->locations.size();
    for (int nr=0; nr<n; nr++)
        for (auto&& r : ses->routes[nr])
        {
            num2str(num, r.distance);
            cfprintf(myfile, (*(r.cond))
                    ?"%croute {%s} {%s} {%s} %s {%s}\n"
                    :"%croute {%s} {%s} {%s} %s\n",
                    tintin_char,
                    ses->locations[nr],
                    ses->locations[r.dest],
                    r.path,
                    num,
                    r.cond);
        };

    hl = hash2list(ses->binds, 0);
    end = &hl->pairs[0] + hl->size;
    for (struct pair *n = &hl->pairs[0]; n<end; n++)
        cfcom(myfile, "bind", n->left, n->right, 0);
    delete[] hl;

    for (int nr=0;nr<NHOOKS;nr++)
        if (ses->hooks[nr])
            cfcom(myfile, "hook", hook_names[nr], ses->hooks[nr], 0);

    fclose(myfile);
    tintin_printf(ses, "#COMMANDS-FILE WRITTEN.");
    return;
}


static bool route_exists(const char *A, const char *B, const char *path, num_t dist, const char *cond, session *ses)
{
    int a, b;

    int n = ses->locations.size();
    for (a=0; a<n; a++)
        if (ses->locations[a]&&!strcmp(ses->locations[a], A))
            break;
    if (a == n)
        return false;
    for (b=0; b<n; b++)
        if (ses->locations[b]&&!strcmp(ses->locations[b], B))
            break;
    if (b == n)
        return false;
    for (auto&& r : ses->routes[a])
        if ((r.dest ==b )&&
                (!strcmp(r.path, path))&&
                (r.distance==dist)&&
                (!strcmp(r.cond, cond)))
            return true;
    return false;
}

static void ws_hash(struct hashtable *h1, struct hashtable *h0, const char *what, FILE *f)
{
    struct pairlist *pl = hash2list(h1, 0);
    struct pair *end = &pl->pairs[0] + pl->size;
    for (struct pair *n = &pl->pairs[0]; n<end; n++)
    {
        char *val;
        if ((val=get_hash(h0, n->left)))
            if (!strcmp(val, n->right))
                continue;
        cfcom(f, what, n->left, n->right, 0);
    }
    delete[] pl;
}

/*****************************/
/* the #writesession command */
/*****************************/
void writesession_command(const char *filename, session *ses)
{
    FILE *myfile;
    char buffer[BUFFER_SIZE*4], num[32], fname[BUFFER_SIZE], lfname[BUFFER_SIZE];

    if (ses==nullsession)
        return tintin_eprintf(ses, "#THIS IS THE NULL SESSION -- NO DELTA!");

    get_arg_in_braces(filename, buffer, 1);
    substitute_vars(buffer, buffer, ses);
    expand_filename(buffer, fname, lfname);
    if (!*filename)
        return tintin_eprintf(ses, "#ERROR - COULDN'T OPEN FILE {%s}.", fname);
    if (!(myfile = fopen(lfname, "w")))
        return tintin_eprintf(ses, "#ERROR - COULDN'T OPEN FILE {%s}.", fname);

#undef SFLAG
#define SFLAG(name, var, dummy) WFLAG(name, ses->var, nullsession->var);
    SFLAG("echo", echo, DEFAULT_ECHO);
    SFLAG("ignore", ignore, DEFAULT_IGNORE);
    SFLAG("speedwalk", speedwalk, DEFAULT_SPEEDWALK);
    SFLAG("presub", presub, DEFAULT_PRESUB);
    SFLAG("togglesubs", togglesubs, DEFAULT_TOGGLESUBS);
    SFLAG("verbose", verbose, false);
    SFLAG("blank", blank, DEFAULT_DISPLAY_BLANK);
    SFLAG("messages aliases", mesvar[MSG_ALIAS], DEFAULT_ALIAS_MESS);
    SFLAG("messages actions", mesvar[MSG_ACTION], DEFAULT_ACTION_MESS);
    SFLAG("messages substitutes", mesvar[MSG_SUBSTITUTE], DEFAULT_SUB_MESS);
    SFLAG("messages events", mesvar[MSG_EVENT], DEFAULT_EVENT_MESS);
    SFLAG("messages highlights", mesvar[MSG_HIGHLIGHT], DEFAULT_HIGHLIGHT_MESS);
    SFLAG("messages variables", mesvar[MSG_VARIABLE], DEFAULT_VARIABLE_MESS);
    SFLAG("messages routes", mesvar[MSG_ROUTE], DEFAULT_ROUTE_MESS);
    SFLAG("messages gotos", mesvar[MSG_GOTO], DEFAULT_GOTO_MESS);
    SFLAG("messages binds", mesvar[MSG_BIND], DEFAULT_BIND_MESS);
    SFLAG("messages #system", mesvar[MSG_SYSTEM], DEFAULT_SYSTEM_MESS);
    SFLAG("messages paths", mesvar[MSG_PATH], DEFAULT_PATH_MESS);
    SFLAG("messages errors", mesvar[MSG_ERROR], DEFAULT_ERROR_MESS);
    SFLAG("messages hooks", mesvar[MSG_HOOK], DEFAULT_HOOK_MESS);
    SFLAG("messages log", mesvar[MSG_LOG], DEFAULT_LOG_MESS);
    SFLAG("messages ticks", mesvar[MSG_TICK], DEFAULT_TICK_MESS);
    SFLAG("verbatim", verbatim, false);
    if (ses->tick_size != DEFAULT_TICK_SIZE*NANO)
    {
        usecstr(num, ses->tick_size);
        cfprintf(myfile, "%cticksize %s\n", tintin_char, num);
    }
    if (ses->pretick != DEFAULT_PRETICK*NANO)
    {
        usecstr(num, ses->pretick);
        cfprintf(myfile, "%cpretick %s\n", tintin_char, num);
    }
    if (strcmp(nullsession->charset, ses->charset))
        cfprintf(myfile, "%ccharset {%s}\n", tintin_char, ses->charset);
    if (strcmp(logcs_name(nullsession->logcharset), logcs_name(ses->logcharset)))
        cfprintf(myfile, "%clogcharset {%s}\n", tintin_char, logcs_name(ses->logcharset));

    ws_hash(ses->aliases, nullsession->aliases, "alias", myfile);

    TRIP_ITER(ses->actions, n)
        ptrip *m = kb_get(trip, nullsession->actions, n);
        if (m && !strcmp(n->right, (*m)->right))
            continue;
        cfcom(myfile, "action", n->left, n->right, n->pr);
    ENDITER

    TRIP_ITER(ses->prompts, n)
        ptrip *m = kb_get(trip, nullsession->prompts, n);
        if (m && !strcmp(n->right, (*m)->right))
            continue;
        cfcom(myfile, "promptaction", n->left, n->right, n->pr);
    ENDITER

    for (const auto p : ses->antisubs)
        if (!nullsession->antisubs.count(p))
            cfcom(myfile, "antisub", p, 0, 0);

    for (const auto& n : ses->subs)
    {
        const auto& m = nullsession->subs.find(n.first);
        if (m!=nullsession->subs.cend() && !strcmp(n.second, m->second))
            continue;
        if (strcmp(n.second, EMPTY_LINE))
            cfcom(myfile, "sub", n.first, n.second, 0);
        else
            cfcom(myfile, "gag", n.first, 0, 0);
    }

    ws_hash(ses->myvars, nullsession->myvars, "var", myfile);

    for (const auto& n : ses->highs)
    {
        const auto& m = nullsession->highs.find(n.first);
        if (m!=nullsession->highs.cend() && !strcmp(n.second, m->second))
            continue;
        cfcom(myfile, "highlight", n.first, n.second, 0);
    }

    ws_hash(ses->pathdirs, nullsession->pathdirs, "pathdir", myfile);

    int n = ses->locations.size();
    for (int nr=0 ;nr<n; nr++)
        for (auto&& r : ses->routes[nr])
        {
            if (!route_exists(ses->locations[nr],
                              ses->locations[r.dest],
                              r.path,
                              r.distance,
                              r.cond,
                              nullsession))
            {
                num2str(num, r.distance);
                cfprintf(myfile, (*(r.cond))
                        ?"%croute {%s} {%s} {%s} %s {%s}\n"
                        :"%croute {%s} {%s} {%s} %s\n",
                        tintin_char,
                        ses->locations[nr],
                        ses->locations[r.dest],
                        r.path,
                        num,
                        r.cond);
            }
        };

    ws_hash(ses->binds, nullsession->binds, "bind", myfile);

    for (int nr=0;nr<NHOOKS;nr++)
        if (ses->hooks[nr])
            if (!nullsession->hooks[nr] ||
                strcmp(ses->hooks[nr], nullsession->hooks[nr]))
            {
                cfcom(myfile, "hook", hook_names[nr], ses->hooks[nr], 0);
            }

    fclose(myfile);
    tintin_printf(ses, "#DELTA COMMANDS FILE WRITTEN.");
    return;
}


static void cfcom(FILE *f, const char *command, const char *left, const char *right, const char *pr)
{
    char buffer[BUFFER_SIZE*4], *b = buffer;
    /* "result" is four times as big as the regular buffer.  This is */
    /* pointless as read line are capped at 1/2 buffer anyway.       */
    b+=sprintf(b, "%c%s {%s}", tintin_char, command, left);
    if (right)
        b+=sprintf(b, " {%s}", right);
    if (pr && strlen(pr))
        b+=sprintf(b, " {%s}", pr);
    *b++='\n'; *b=0;
    cfputs(buffer, f);
}


/**********************************/
/* load a file for input to mud.  */
/**********************************/
void textin_command(const char *arg, session *ses)
{
    FILE *myfile;
    char buffer[BUFFER_SIZE], filename[BUFFER_SIZE], *cptr, lfname[BUFFER_SIZE];
    mbstate_t cs;

    ZERO(cs);

    get_arg_in_braces(arg, buffer, 1);
    substitute_vars(buffer, buffer, ses);
    expand_filename(buffer, filename, lfname);
    if (ses == nullsession)
    {
        return tintin_eprintf(ses,
            "#You can't read any text in without a session being active.");
    }
    if (!(myfile = fopen(lfname, "r")))
        return tintin_eprintf(ses, "ERROR: File {%s} doesn't exist.", filename);
    while (fgets(buffer, sizeof(buffer), myfile))
    {
        for (cptr = buffer; *cptr && *cptr != '\n'; cptr++) ;
        *cptr = '\0';
        local_to_utf8(lfname, buffer, BUFFER_SIZE, &cs);
        write_line_mud(lfname, ses);
    }
    fclose(myfile);
    tintin_printf(ses, "#File read - Success.");
}

const char *logtypes[]=
{
    "raw",
    "lf",
    "ttyrec",
};

/************************/
/* the #logtype command */
/************************/
void logtype_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE];

    arg=get_arg(arg, left, 1, ses);
    if (!*left)
        return tintin_printf(ses, "#The log type is: %s", logtypes[ses->logtype]);
    for (unsigned t=0; t<ARRAYSZ(logtypes); t++)
        if (is_abrev(left, logtypes[t]))
        {
            ses->logtype = static_cast<logtype_t>(t);
            tintin_printf(MSG_LOG, ses, "#Ok, log type is now %s.", logtypes[t]);
            return;
        }
    tintin_eprintf(ses, "#No such logtype: {%s}", left);
}

/***************************/
/* the #logcharset command */
/***************************/
void logcharset_command(const char *arg, session *ses)
{
    char what[BUFFER_SIZE], *cset;
    struct charset_conv nc;

    get_arg(arg, what, 1, ses);
    cset=what;

    if (!*cset)
        return tintin_printf(ses, "#Log charset: %s", logcs_name(ses->logcharset));
    if (!strcasecmp(cset, "local"))
        cset=LOGCS_LOCAL;
    else if (!strcasecmp(cset, "remote"))
        cset=LOGCS_REMOTE;
    if (!new_conv(&nc, logcs_charset(cset), 1))
        return tintin_eprintf(ses, "#No such charset: {%s}", logcs_charset(cset));
    if (!logcs_is_special(ses->logcharset))
        SFREE(ses->logcharset);
    ses->logcharset=logcs_is_special(cset) ? cset : mystrdup(cset);
    if (ses!=nullsession && ses->logfile)
    {
        cleanup_conv(&ses->c_log);
        memcpy(&ses->c_log, &nc, sizeof(nc));
    }
    else
        cleanup_conv(&nc);
    tintin_printf(MSG_LOG, ses, "#Log charset set to %s", logcs_name(cset));
}
