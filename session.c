/*********************************************************************/
/* file: session.c - functions related to sessions                   */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include "protos/action.h"
#include "protos/colors.h"
#include "protos/events.h"
#include "protos/files.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/hooks.h"
#include "protos/lists.h"
#include "protos/print.h"
#include "protos/math.h"
#include "protos/net.h"
#include "protos/parse.h"
#include "protos/routes.h"
#include "protos/run.h"
#include "protos/slist.h"
#include "protos/unicode.h"
#include "protos/user.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/tlist.h"
#ifdef HAVE_GNUTLS
#include "protos/ssl.h"
#else
# define gnutls_session_t int
#endif


#ifdef HAVE_GNUTLS
#else
# define gnutls_session_t int
#endif

static struct session *new_session(const char *name, const char *address, int fd, sestype_t sestype, gnutls_session_t ssl, struct session *ses);
static void show_session(struct session *ses);

static bool session_exists(char *name)
{
    for (struct session *sesptr = sessionlist; sesptr; sesptr = sesptr->next)
        if (!strcmp(sesptr->name, name))
            return true;
    return false;
}

/* FIXME: use non-ascii letters in generated names */

/* NOTE: basis is in the local charset, not UTF-8 */
void make_name(char *str, const char *basis)
{
    char *t;
    int i, j;

    for (const char *b=basis; (*b=='/')||is7alnum(*b)||(*b=='_'); b++)
        if (*b=='/')
            basis=b+1;
    if (!is7alpha(*basis))
        goto noname;
    strlcpy(str, basis, MAX_SESNAME_LENGTH);
    for (t=str; is7alnum(*t)||(*t=='_'); t++);
    *t=0;
    if (!session_exists(str))
        return;
    i=2;
    do sprintf(t, "%d", i++); while (session_exists(str));
    return;
noname:
    for (i=1; ; i++)
    {
        j=i;
        *(t=str+10)=0;
        do
        {
            *--t='a'+(j-1)%('z'+1-'a');
            j=(j-1)/('z'+1-'a');
        } while (j);
        if (!session_exists(t))
        {
            strcpy(str, t);
            return;
        }
    }
}


/*
  common code for #session and #run for cases of:
    #session            - list all sessions
    #session {a}        - print info about session a
  (opposed to #session {a} {mud.address.here 666} - starting a new session)
*/
static int list_sessions(const char *arg, struct session *ses, char *left, char *right)
{
    struct session *sesptr;
    arg = get_arg_in_braces(arg, left, 0);
    arg = get_arg_in_braces(arg, right, 1);

    if (!*left)
    {
        tintin_puts("#THESE SESSIONS HAS BEEN DEFINED:", ses);
        for (sesptr = sessionlist; sesptr; sesptr = sesptr->next)
            if (sesptr!=nullsession)
                show_session(sesptr);
    }
    else if (*left && !*right)
    {
        for (sesptr = sessionlist; sesptr; sesptr = sesptr->next)
            if (!strcmp(sesptr->name, left))
            {
                show_session(sesptr);
                break;
            }
        if (!sesptr)
            tintin_puts("#THAT SESSION IS NOT DEFINED.", ses);
    }
    else
    {
        if (strlen(left) > MAX_SESNAME_LENGTH)
        {
            tintin_eprintf(ses, "#SESSION NAME TOO LONG.");
            return 1;
        }
        if (session_exists(left))
        {
            tintin_eprintf(ses, "#THERE'S A SESSION WITH THAT NAME ALREADY.");
            return 1;
        }
        return 0;
    }
    return 1;
}

/*****************************************/
/* the #session and #sslsession commands */
/*****************************************/
static struct session *socket_session(const char *arg, struct session *ses, bool ssl)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], host[BUFFER_SIZE];
    int sock;
    char *port;
#ifdef HAVE_GNUTLS
    gnutls_session_t sslses;
#endif

    if (list_sessions(arg, ses, left, right))
        return ses;     /* (!*left)||(!*right) */

    strcpy(host, space_out(right));

    if (!*host)
    {
        tintin_eprintf(ses, "#session: HEY! SPECIFY AN ADDRESS WILL YOU?");
        return ses;
    }

    port=host;
    while (*port && !isaspace(*port))
        port++;
    if (*port)
    {
        *port++ = '\0';
        port = (char*)space_out(port);
    }

    if (!*port)
    {
        tintin_eprintf(ses, "#session: HEY! SPECIFY A PORT NUMBER WILL YOU?");
        return ses;
    }
    if (!(sock = connect_mud(host, port, ses)))
        return ses;

#ifdef HAVE_GNUTLS
    if (ssl)
    {
        if (!(sslses=ssl_negotiate(sock, host, ses)))
        {
            close(sock);
            return ses;
        }
        return new_session(left, right, sock, SES_SOCKET, sslses, ses);
    }
#endif
    return new_session(left, right, sock, SES_SOCKET, 0, ses);
}


struct session *session_command(const char *arg, struct session *ses)
{
    return socket_session(arg, ses, false);
}

struct session *sslsession_command(const char *arg, struct session *ses)
{
#ifdef HAVE_GNUTLS
    return socket_session(arg, ses, true);
#else
    tintin_eprintf(ses, "#SSLSESSION is not supported.  Please recompile KBtin against GnuTLS.");
    return ses;
#endif
}


/********************/
/* the #run command */
/********************/
struct session *run_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], ustr[BUFFER_SIZE];
    int sock;

    if (list_sessions(arg, ses, left, right))
        return ses;     /* (!*left)||(!*right) */

    if (!*right)
    {
        tintin_eprintf(ses, "#run: HEY! SPECIFY A COMMAND, WILL YOU?");
        return ses;
    }

    if (!strcmp(right, "//selfpipe"))
    {
        int p[2];
        if (pipe(p))
            syserr("pipe failed");
        ses = new_session(left, right, p[0], SES_SELFPIPE, 0, ses);
        ses->nagle = p[1];
        return ses;
    }

    utf8_to_local(ustr, right);
    if ((sock=run(ustr, COLS, LINES-1, TERM)) == -1)
    {
        tintin_eprintf(ses, "#forkpty() FAILED: %s", strerror(errno));
        return ses;
    }

    return new_session(left, right, sock, SES_PTY, 0, ses);
}


/******************/
/* show a session */
/******************/
static void show_session(struct session *ses)
{
    tintin_printf(0, "%-10s{%s}%s%s%s", ses->name, ses->address,
        ses == activesession ? " (active)":"",
        ses->snoopstatus ? " (snooped)":"",
        ses->logfile ? " (logging)":"");
}

/**********************************/
/* find a new session to activate */
/**********************************/
struct session* newactive_session(void)
{
    if ((activesession=sessionlist)==nullsession)
        activesession=activesession->next;
    if (activesession)
    {
        char buf[BUFFER_SIZE];

        sprintf(buf, "#SESSION '%s' ACTIVATED.", activesession->name);
        tintin_puts1(buf, activesession);
        return do_hook(activesession, HOOK_ACTIVATE, 0, false);
    }
    else
    {
        activesession=nullsession;
        tintin_puts1("#THERE'S NO ACTIVE SESSION NOW.", activesession);
        return do_hook(activesession, HOOK_ACTIVATE, 0, false);
    }
}


/*********************************************/
/* clear all lists associated with a session */
/*********************************************/
void kill_all(struct session *ses, bool no_reinit)
{
    if (!ses) // can't happen
        return;

    kill_hash(ses->aliases);
    kill_tlist(ses->actions);
    kill_tlist(ses->prompts);
    kill_hash(ses->myvars);
    kill_tlist(ses->highs);
    kill_tlist(ses->subs);
    kill_slist(ses->antisubs);
    for (int i=0; i<MAX_PATH_LENGTH; i++)
        free((char*)ses->path[i].left), free((char*)ses->path[i].right);
    kill_hash(ses->pathdirs);
    kill_hash(ses->binds);
    kill_routes(ses);
    kill_events(ses);
    if (no_reinit)
        return;

    ses->aliases = init_hash();
    ses->actions = init_tlist();
    ses->prompts = init_tlist();
    ses->myvars = init_hash();
    ses->highs = init_tlist();
    ses->subs = init_tlist();
    ses->antisubs = init_slist();
    ses->binds = init_hash();
    ses->path_begin = ses->path_length = 0;
    bzero(ses->path, sizeof(ses->path));
    ses->pathdirs = init_hash();
#ifdef HAVE_HS
    ses->highs_dirty = true;
#endif
    tintin_printf(ses, "#Lists cleared.");
}


/*******************************/
/* initialize the null session */
/*******************************/
void init_nullses(void)
{
    start_time = idle_since = current_time();

    nullsession=TALLOC(struct session);
    nullsession->name=mystrdup("main");
    nullsession->address=0;
    nullsession->tickstatus = false;
    nullsession->tick_size = DEFAULT_TICK_SIZE*NANO;
    nullsession->pretick = DEFAULT_PRETICK*NANO;
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
    nullsession->actions = init_tlist();
    nullsession->prompts = init_tlist();
    nullsession->subs = init_tlist();
    nullsession->myvars = init_hash();
    nullsession->highs = init_tlist();
    nullsession->pathdirs = init_hash();
    nullsession->socket = 0;
    nullsession->sestype = SES_NULL;
    nullsession->naws = false;
#ifdef HAVE_ZLIB
    nullsession->can_mccp = false;
    nullsession->mccp = 0;
    nullsession->mccp_more = false;
#endif
    nullsession->last_term_type=0;
    nullsession->server_echo = 0;
    nullsession->nagle = false;
    nullsession->antisubs = init_slist();
    nullsession->binds = init_hash();
    nullsession->next = 0;
    nullsession->sessionstart = nullsession->idle_since =
        nullsession->server_idle_since = start_time;
    nullsession->debuglogfile=0;
    nullsession->debuglogname=0;
    for (int i=0;i<HISTORY_SIZE;i++)
        history[i]=0;
    nullsession->routes=0;
    nullsession->locations=0;
    nullsession->num_locations=0;
    for (int i=0;i<NHOOKS;i++)
        nullsession->hooks[i]=0;
    nullsession->path_begin = 0;
    nullsession->path_length = 0;
    bzero(nullsession->path, sizeof(nullsession->path));
    nullsession->last_line[0] = 0;
    nullsession->linenum = 0;
    nullsession->events = NULL;
    nullsession->verbose=false;
    nullsession->closing=false;
    nullsession->drafted=false;
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
    nullsession->mesvar[MSG_TICK]= DEFAULT_TICK_MESS;
    nullsession->charset=mystrdup(DEFAULT_CHARSET);
    nullsession->logcharset=logcs_is_special(DEFAULT_LOGCHARSET) ?
                              DEFAULT_LOGCHARSET : mystrdup(DEFAULT_LOGCHARSET);
    nullify_conv(&nullsession->c_io);
    nullify_conv(&nullsession->c_log);
    nullsession->line_time=0;
#ifdef HAVE_GNUTLS
    nullsession->ssl=0;
#endif
#ifdef HAVE_HS
    nullsession->highs_dirty=false;
    nullsession->act_dirty[0]=nullsession->act_dirty[1]=false;
    nullsession->subs_dirty=false;
    nullsession->antisubs_dirty=false;
    nullsession->highs_hs=0;
    nullsession->acts_hs[0]=nullsession->acts_hs[1]=0;
    nullsession->subs_hs=0;
    nullsession->antisubs_hs=0;
    nullsession->highs_cols=0;
    nullsession->subs_data=0;
    nullsession->subs_markers=0;
#endif
}


/**********************/
/* open a new session */
/**********************/
static struct session *new_session(const char *name, const char *address, int sock, sestype_t sestype, gnutls_session_t ssl, struct session *ses)
{
    struct session *newsession;

    newsession = TALLOC(struct session);

    newsession->name = mystrdup(name);
    newsession->address = mystrdup(address);
    newsession->tickstatus = false;
    newsession->tick_size = ses->tick_size;
    newsession->pretick = ses->pretick;
    newsession->time0 = 0;
    newsession->snoopstatus = false;
    newsession->logfile = 0;
    newsession->logname = 0;
    newsession->logtype = ses->logtype;
    newsession->loginputprefix = mystrdup(ses->loginputprefix);
    newsession->loginputsuffix = mystrdup(ses->loginputsuffix);
    newsession->ignore = ses->ignore;
    newsession->aliases = copy_hash(ses->aliases);
    newsession->actions = copy_tlist(ses->actions);
    newsession->prompts = copy_tlist(ses->prompts);
    newsession->subs = copy_tlist(ses->subs);
    newsession->myvars = copy_hash(ses->myvars);
    newsession->highs = copy_tlist(ses->highs);
    newsession->pathdirs = copy_hash(ses->pathdirs);
    newsession->socket = sock;
    newsession->antisubs = copy_slist(ses->antisubs);
    newsession->binds = copy_hash(ses->binds);
    newsession->sestype = sestype;
    newsession->naws = (sestype == SES_PTY);
#ifdef HAVE_ZLIB
    newsession->can_mccp = false;
    newsession->mccp = 0;
    newsession->mccp_more = false;
#endif
    newsession->ga = false;
    newsession->gas = false;
    newsession->server_echo = 0;
    newsession->telnet_buflen = 0;
    newsession->last_term_type = 0;
    newsession->next = sessionlist;
    newsession->path_begin = 0;
    newsession->path_length = 0;
    bzero(newsession->path, sizeof(newsession->path));
    newsession->more_coming = false;
    newsession->events = NULL;
    newsession->verbose = ses->verbose;
    newsession->blank = ses->blank;
    newsession->echo = ses->echo;
    newsession->speedwalk = ses->speedwalk;
    newsession->togglesubs = ses->togglesubs;
    newsession->presub = ses->presub;
    newsession->verbatim = ses->verbatim;
    newsession->sessionstart=newsession->idle_since=
        newsession->server_idle_since=current_time();
    newsession->nagle=false;
    newsession->halfcr_in=false;
    newsession->halfcr_log=false;
    newsession->lastintitle=0;
    newsession->debuglogfile=0;
    newsession->debuglogname=0;
    newsession->linenum=0;
    newsession->drafted=false;
    newsession->partial_line_marker = mystrdup(ses->partial_line_marker);
    for (int i=0;i<=MAX_MESVAR;i++)
        newsession->mesvar[i] = ses->mesvar[i];
    newsession->routes=0;
    newsession->locations=0;
    newsession->num_locations=0;
    copyroutes(ses, newsession);
    newsession->last_line[0]=0;
    for (int i=0;i<NHOOKS;i++)
        if (ses->hooks[i])
            newsession->hooks[i]=mystrdup(ses->hooks[i]);
        else
            newsession->hooks[i]=0;
    newsession->closing=0;
    newsession->charset = mystrdup(sestype==SES_SOCKET ? ses->charset : user_charset_name);
    newsession->logcharset = logcs_is_special(ses->logcharset) ?
                              ses->logcharset : mystrdup(ses->logcharset);
    if (!new_conv(&newsession->c_io, newsession->charset, 0))
        tintin_eprintf(0, "#Warning: can't open charset: %s", newsession->charset);
    nullify_conv(&newsession->c_log);
    newsession->line_time=0;
#ifdef HAVE_GNUTLS
    newsession->ssl=ssl;
#endif
#ifdef HAVE_HS
    newsession->highs_dirty=true;
    newsession->act_dirty[0]=newsession->act_dirty[1]=true;
    newsession->subs_dirty=true;
    newsession->antisubs_dirty=true;
    newsession->highs_hs=0;
    newsession->acts_hs[0]=newsession->acts_hs[1]=0;
    newsession->subs_hs=0;
    newsession->antisubs_hs=0;
    newsession->highs_cols=0;
    newsession->subs_data=0;
    newsession->subs_markers=0;
#endif
    sessionlist = newsession;
    activesession = newsession;

    return do_hook(newsession, HOOK_OPEN, 0, false);
}


/*****************************************************************************/
/* cleanup after session died. if session=activesession, try find new active */
/*****************************************************************************/
void cleanup_session(struct session *ses)
{
    char buf[BUFFER_SIZE+1];
    struct session *sesptr, *act;

    if (ses->closing)
        return;
    any_closed=true;
    ses->closing=2;
    if (ses!=nullsession) /* valgrind cleans null */
        do_hook(act=ses, HOOK_CLOSE, 0, true);

    if (ses->logfile)
        log_off(ses);
    if (ses->debuglogfile)
        fclose(ses->debuglogfile);

    kill_all(ses, true);
    if (ses == sessionlist)
        sessionlist = ses->next;
    else
    {
        for (sesptr = sessionlist; sesptr->next != ses; sesptr = sesptr->next) ;
        sesptr->next = ses->next;
    }
    if (ses==activesession && ses!=nullsession)
    {
        user_textout_draft(0, 0);
        sprintf(buf, "%s\n", ses->last_line);
        convert(&ses->c_io, ses->last_line, buf, -1);
        do_in_MUD_colors(ses->last_line, false, 0);
        user_textout(ses->last_line);
    }
    if (ses!=nullsession) /* valgrind cleans null */
        tintin_printf(0, "#SESSION '%s' DIED.", ses->name);
    if (ses->socket && close(ses->socket) == -1)
        syserr("close in cleanup");
    SFREE(ses->loginputprefix);
    SFREE(ses->loginputsuffix);
    for (int i=0;i<NHOOKS;i++)
        SFREE(ses->hooks[i]);
    SFREE(ses->name);
    SFREE(ses->address);
    SFREE(ses->partial_line_marker);
    cleanup_conv(&ses->c_io);
    SFREE(ses->charset);
    if (!logcs_is_special(ses->logcharset))
        SFREE(ses->logcharset);
#ifdef HAVE_ZLIB
    if (ses->mccp)
    {
        inflateEnd(ses->mccp);
        TFREE(ses->mccp, z_stream);
    }
#endif
#ifdef HAVE_GNUTLS
    if (ses->ssl)
        gnutls_deinit(ses->ssl);
#endif
#ifdef HAVE_HS
    hs_free_database(ses->highs_hs);
    kill_acts(ses, 0);
    kill_acts(ses, 1);
    hs_free_database(ses->subs_hs);
    hs_free_database(ses->antisubs_hs);
    free(ses->highs_cols);
    free(ses->subs_data);
    free(ses->subs_markers);
#endif

    TFREE(ses, struct session);
}

void seslist(char *result)
{
    bool flag=false;
    char *r0=result;

    if (sessionlist==nullsession && !nullsession->next)
        return;

    for (struct session *sesptr = sessionlist; sesptr; sesptr = sesptr->next)
        if (sesptr!=nullsession)
        {
            if (flag)
                *result++=' ';
            else
                flag=true;
            result+=snprintf(result, BUFFER_SIZE-5+r0-result,
                isatom(sesptr->name) ? "%s" : "{%s}", sesptr->name);
            if (result-r0>BUFFER_SIZE-10)
                return; /* pathological session names */
        }
}
