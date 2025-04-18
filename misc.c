/*********************************************************************/
/* file: misc.c - misc commands                                      */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include <sys/stat.h>
#include <sys/time.h>
#ifdef HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif
#include "protos/bind.h"
#include "protos/colors.h"
#include "protos/files.h"
#include "protos/globals.h"
#include "protos/highlight.h"
#include "protos/history.h"
#include "protos/hooks.h"
#include "protos/print.h"
#include "protos/math.h"
#include "protos/mudcolors.h"
#include "protos/net.h"
#include "protos/parse.h"
#include "protos/routes.h"
#include "protos/run.h"
#include "protos/session.h"
#include "protos/slist.h"
#include "protos/substitute.h"
#include "protos/unicode.h"
#include "protos/user.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/tlist.h"
#include "protos/vars.h"


/* externs */
bool margins;
int marginl, marginr;


int yes_no(const char *txt)
{
    if (!*txt)
        return -2;
    if (!strcmp(txt, "0"))
        return 0;
    if (!strcmp(txt, "1"))
        return 1;
    if (!strcasecmp(txt, "NO"))
        return 0;
    if (!strcasecmp(txt, "YES"))
        return 1;
    if (!strcasecmp(txt, "OFF"))
        return 0;
    if (!strcasecmp(txt, "ON"))
        return 1;
    if (!strcasecmp(txt, "FALSE"))
        return 0;
    if (!strcasecmp(txt, "TRUE"))
        return 1;
    return -1;
}

static void togglebool(bool *b, const char *arg, struct session *ses, const char *msg1, const char *msg2)
{
    char tmp[BUFFER_SIZE];
    int old=*b;

    get_arg(arg, tmp, 1, ses);
    if (*tmp)
    {
        switch (yes_no(tmp))
        {
        case 0:
            *b=false; break;
        case 1:
            *b=true; break;
        default:
            tintin_eprintf(ses, "#Valid boolean values are: 1/0, YES/NO, TRUE/FALSE, ON/OFF. Got {%s}.", tmp);
        }
    }
    else
        *b=!*b;
    if (*b!=old)
        tintin_printf(ses, *b?msg1:msg2);
}

/****************************/
/* the #cr command          */
/****************************/
void cr_command(const char *arg, struct session *ses)
{
    if (ses != nullsession)
        write_line_mud("", ses);
}

/****************************/
/* the #version command     */
/****************************/
void version_command(const char *arg, struct session *ses)
{
    tintin_printf(ses, "#You are using KBtin %s", VERSION);
}

/****************************/
/* the #verbatim command    */
/****************************/
void verbatim_command(const char *arg, struct session *ses)
{
    togglebool(&ses->verbatim, arg, ses,
               "#All text is now sent 'as is'.",
               "#Text is no longer sent 'as is'.");
}

/************************/
/* the #send command    */
/************************/
void send_command(const char *arg, struct session *ses)
{
    char temp1[BUFFER_SIZE];
    if (ses==nullsession)
    {
        tintin_eprintf(ses, "#No session -> can't #send anything");
        return;
    }
    if (!*arg)
    {
        tintin_eprintf(ses, "#send what?");
        return;
    }
    arg = get_arg(arg, temp1, 1, ses);
    write_line_mud(temp1, ses);
}

/****************************/
/* the #sendchar command    */
/****************************/
void sendchar_command(const char *arg, struct session *ses)
{
    char chdesc[BUFFER_SIZE], outbuf[BUFFER_SIZE], *ep, *outp=outbuf;
    long int ch;
    if (ses==nullsession)
    {
        tintin_eprintf(ses, "#No session -> can't #sendchar anything");
        return;
    }
    if (!*arg)
    {
        tintin_eprintf(ses, "#sendchar what?");
        return;
    }
    while (1)
    {
        arg=get_arg(arg, chdesc, 0, ses);
        if (!*arg && !*chdesc)
            break;
        for (char *chp=chdesc; *chp; chp++)
        {
            switch (*chp)
            {
            case '^':
                chp++;
                if (*chp>='@' && *chp<='_')
                    *outp++=*chp-64;
                else if (*chp>='a' && *chp<='z')
                    *outp++=*chp-96;
                else
                    tintin_eprintf(ses, "#sendchar: invalid ^ char at {^%s}", chp);
                break;
            case '\\':
                chp++;
                switch (*chp)
                {
                case '"':
                    *outp++='"'; break;
                case '\\':
                    *outp++='\\'; break;
                case 'a':
                    *outp++='\a'; break;
                case 'b':
                    *outp++='\b'; break;
                case 'e':
                    *outp++='\033'; break;
                case 'f':
                    *outp++='\f'; break;
                case 'n':
                    *outp++='\n'; break;
                case 'r':
                    *outp++='\r'; break;
                case 't':
                    *outp++='\t'; break;
                case 'v':
                    *outp++='\v'; break;
                case '0':
                    ch=strtol(chp, &ep, 8);
                uchar:
                    if (ch<0 || ch>0x10ffff)
                        tintin_eprintf(ses, "#sendchar: code %lx out of Unicode at {\\%s}", ch, chp);
                    else
                    {
                        wchar_t wch=ch;
                        outp+=wc_to_utf8(outp, &wch, 1, outbuf-outp+BUFFER_SIZE);
                    }
                    chp=ep-1;
                    break;
                case 'x':
                    ch=strtol(chp, &ep, 16);
                    goto uchar;
                default:
                    tintin_eprintf(ses, "#sendchar: unknown escape at {\\%s}", chp);
                }
                break;
            case 'U':
                chp++;
                if (*chp=='+')
                    chp++;
                ch=strtol(chp, &ep, 16);
                if (ch<0 || ch>0x10ffff)
                    tintin_eprintf(ses, "#sendchar: code %lx out of Unicode at {U%s}", ch, chp);
                else
                {
                    wchar_t wch=ch;
                    outp+=wc_to_utf8(outp, &wch, 1, outbuf-outp+BUFFER_SIZE);
                }
                chp=ep-1;
                break;
            default:
                *outp++=*chp;
            }
        }
    }
    *outp=0;
    write_raw_mud(outbuf, outp-outbuf, ses);
}

/********************/
/* the #all command */
/********************/
struct session* all_command(const char *arg, struct session *ses)
{
    if ((sessionlist!=nullsession)||(nullsession->next))
    {
        char what[BUFFER_SIZE];
        get_arg(arg, what, 1, ses);
        for (struct session *sesptr = sessionlist; sesptr; sesptr = sesptr->next)
            if (sesptr!=nullsession)
                parse_input(what, true, sesptr);
    }
    else
        tintin_eprintf(ses, "#all: BUT THERE ISN'T ANY SESSION AT ALL!");
    return ses;
}

/*********************/
/* the #bell command */
/*********************/
void bell_command(const char *arg, struct session *ses)
{
    user_beep();
}

void beep_command(const char *arg, struct session *ses)
{
    user_beep();
}


/*********************/
/* the #char command */
/*********************/
void char_command(const char *arg, struct session *ses)
{
    char what[BUFFER_SIZE];
    get_arg_in_braces(arg, what, 1);
    /* It doesn't make any sense to use a variable here. */
    if (is7punct(*what))
    {
        tintin_char = *arg;
        tintin_printf(ses, "#OK. TINTIN-CHAR is now {%c}", tintin_char);
        tintin_char_set = true;
    }
    else
        tintin_eprintf(ses, "#SPECIFY A PROPER TINTIN-CHAR! SOMETHING LIKE # OR /!");
}


/*********************/
/* the #echo command */
/*********************/
void echo_command(const char *arg, struct session *ses)
{
    togglebool(&ses->echo, arg, ses,
               "#ECHO IS NOW ON.",
               "#ECHO IS NOW OFF.");
}

/***********************/
/* the #keypad command */
/***********************/
void keypad_command(const char *arg, struct session *ses)
{
    if (!ui_keyboard)
    {
        tintin_eprintf(ses, "#UI: no access to keyboard => no keybindings");
        return;
    }

    togglebool(&keypad, arg, ses,
               "#KEYPAD NOW WORKS IN THE APPLICATION MODE.",
               "#KEYPAD KEYS ARE NOW EQUAL TO NON-KEYPAD ONES.");
    user_keypad(keypad);
}

/***********************/
/* the #retain command */
/***********************/
void retain_command(const char *arg, struct session *ses)
{
    if (!ui_sep_input)
    {
        tintin_eprintf(ses, "#UI: no managed windows => no input bar");
        return;
    }

    togglebool(&retain, arg, ses,
               "#INPUT BAR WILL NOW RETAIN THE LAST LINE TYPED.",
               "#INPUT BAR WILL NOW BE CLEARED EVERY LINE.");
    user_retain();
}

/*********************/
/* the #end command */
/*********************/
void end_command(const char *arg, struct session *ses)
{
    struct session *sp;

    for (struct session *sesptr = sessionlist; sesptr; sesptr = sp)
    {
        sp = sesptr->next;
        if (sesptr!=nullsession && !sesptr->closing)
        {
            sesptr->closing=1;
            do_hook(sesptr, HOOK_ZAP, 0, true);
            sesptr->closing=0;
            cleanup_session(sesptr);
        }
    }
    activesession = nullsession;
    do_hook(nullsession, HOOK_END, 0, true);
#ifdef HAVE_VALGRIND_VALGRIND_H
    if (RUNNING_ON_VALGRIND)
    {
        cleanup_session(nullsession);
        cleanup_parse();
        cleanup_bind();
        cleanup_history();
    }
# ifdef HAVE_SIMD
    hs_free_scratch(hs_scratch);
# endif
#endif
    activesession = NULL;
    if (ui_own_output)
    {
        tintin_printf(0, "Goodbye!");
        user_done();
    }
    exit(0);
}

/***********************/
/* the #ignore command */
/***********************/
void ignore_command(const char *arg, struct session *ses)
{
    if (ses!=nullsession)
        togglebool(&ses->ignore, arg, ses,
                   "#ACTIONS ARE IGNORED FROM NOW ON.",
                   "#ACTIONS ARE NO LONGER IGNORED.");
    else
        /* don't pierce !#verbose, as this can be set in a config file */
        tintin_printf(ses, "#No session active => Nothing to ignore!");
}

/**********************/
/* the #presub command */
/**********************/
void presub_command(const char *arg, struct session *ses)
{
    togglebool(&ses->presub, arg, ses,
               "#ACTIONS ARE NOW PROCESSED ON SUBSTITUTED BUFFER.",
               "#ACTIONS ARE NO LONGER DONE ON SUBSTITUTED BUFFER.");
}

/**********************/
/* the #blank command */
/**********************/
void blank_command(const char *arg, struct session *ses)
{
    togglebool(&ses->blank, arg, ses,
               "#INCOMING BLANK LINES ARE NOW DISPLAYED.",
               "#INCOMING BLANK LINES ARE NO LONGER DISPLAYED.");
}

/**************************/
/* the #togglesubs command */
/**************************/
void togglesubs_command(const char *arg, struct session *ses)
{
    togglebool(&ses->togglesubs, arg, ses,
               "#SUBSTITUTES ARE NOW IGNORED.",
               "#SUBSTITUTES ARE NO LONGER IGNORED.");
}

/************************/
/* the #verbose command */
/************************/
void verbose_command(const char *arg, struct session *ses)
{
    real_quiet=true;
    togglebool(&ses->verbose, arg, ses,
               "#Output from #reads will now be shown.",
               "#The #read command will no longer output messages.");
    if (in_read)
        puts_echoing=ses->verbose;
}

/************************/
/* the #margins command */
/************************/
void margins_command(const char *arg, struct session *ses)
{
    int l, r;
    char num[BUFFER_SIZE], *tmp;

    if (!ui_sep_input)
    {
        tintin_eprintf(ses, "#UI: no input bar => no input margins");
        return;
    }

    if (margins)
    {
        margins=false;
        tintin_printf(ses, "#MARGINS DISABLED.");
        return;
    }

    l=marginl;
    r=marginr;
    if (*arg)
    {
        arg=get_arg(arg, num, 0, ses);
        if (*num)
        {
            l=strtoul(num, &tmp, 10);
            if (*tmp||(l<=0))
            {
                tintin_eprintf(ses, "#Left margin must be a positive number! Got {%s}.", num);
                return;
            }
            if (l>=BUFFER_SIZE)
            {
                tintin_eprintf(ses, "#Left margin too big (%d)!", l);
                return;
            }
        }
        arg=get_arg(arg, num, 1, ses);
        if (*num)
        {
            r=strtoul(num, &tmp, 10);
            if (*tmp||(r<l))
            {
                tintin_eprintf(ses, "#Right margin must be a number greater than the left margin! Got {%s}.", tmp);
                return;
            }
            if (r>=BUFFER_SIZE)
            {
                tintin_eprintf(ses, "#Right margin too big (%d)!", r);
                return;
            }
        }
    }
    marginl=l;
    marginr=r;
    margins=true;
    tintin_printf(ses, "#MARGINS ENABLED.");
}

/***************************/
/* the #inputcolor command */
/***************************/
// the documented name is "inputcolor" without the s
static const char* iparts[] =
{
    "input",
#if 0
    "retain",
    "margin",
    "scroll",
#endif
};

void inputcolors_command(const char *arg, struct session *ses)
{
    char num[BUFFER_SIZE];

    if (!ui_sep_input)
    {
        tintin_eprintf(ses, "#UI: no input bar to set colors for");
        return;
    }

    get_arg(arg, num, 1, ses);

    if (!*num)
    {
        tintin_printf(ses, "Defined UI colors:");
        for (int i=0; i<(int)ARRAYSZ(iparts); i++)
        {
            char tc[64];
            int c =input_c[i];
            setcolor(tc, c);
            // tc starts and ends in tildes, consume them and escape with ~7~
            tintin_printf(ses, "~7~%-10s %s**~7~ ~~7%s7~~~1~", iparts[i], tc, tc);
        }
        return;
    }

    int c = INPUT_COLOR;
    if (!getcolor(&arg, &c, 0))
    {
        tintin_eprintf(ses, "#Invalid color code, see \"#help color\"");
        return;
    }

    input_c[0] = c;
    user_input_color(c);
}


/***********************/
/* the #showme command */
/***********************/
void showme_command(const char *arg, struct session *ses)
{
    char what[BUFFER_SIZE];
    get_arg(arg, what, 1, ses);
    tintin_printf(ses, "%s", what);        /* KB: no longer check for actions */
}

/***********************/
/* the #loop command   */
/***********************/
struct session *loop_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    int bound1, bound2;
    pvars_t vars, *lastpvars;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg_in_braces(arg, right, 1);
    if (sscanf(left, "%d,%d", &bound1, &bound2) != 2)
    {
        tintin_eprintf(ses, "#Wrong number of arguments in #loop: {%s}.", left);
        return ses;
    }

    if (pvars)
        for (int counter=1; counter<10; counter++)
            strcpy(vars[counter], (*pvars)[counter]);
    else
        for (int counter=1; counter<10; counter++)
            strcpy(vars[counter], "");
    lastpvars=pvars;
    pvars=&vars;

    bool flag = true;
    int counter = bound1;
    while (flag)
    {
        sprintf(vars[0], "%d", counter);
        ses = parse_input(right, true, ses);
        if (bound1 < bound2)
        {
            counter++;
            if (counter > bound2)
                flag = false;
        }
        else
        {
            counter--;
            if (counter < bound2)
                flag = false;
        }
    }
    pvars=lastpvars;
    return ses;
}

static const char *msNAME[]=
{
    "aliases",
    "actions",
    "substitutes",
    "events",
    "highlights",
    "variables",
    "routes",
    "gotos",
    "binds",
    "#system",
    "paths",
    "errors",
    "hooks",
    "logging",
    "ticks",
    "ratelimits",
    "all"
};

/*************************/
/* the #messages command */
/*************************/
void messages_command(const char *arg, struct session *ses)
{
    char offon[2][20];
    char type[BUFFER_SIZE], onoff[BUFFER_SIZE];

    strcpy(offon[0], "OFF.");
    strcpy(offon[1], "ON.");
    arg=get_arg(arg, type, 0, ses);
    arg=get_arg(arg, onoff, 1, ses);
    if (!*type)
    {
        for (int mestype=0;mestype<MAX_MESVAR;++mestype)
            tintin_printf(ses, "#Messages concerning %s are %s",
                    msNAME[mestype], offon[ses->mesvar[mestype]]);
        return;
    }
    int mestype = 0;
    while ((mestype<MAX_MESVAR+1)&&(!is_abrev(type, msNAME[mestype])))
        mestype++;
    if (mestype == MAX_MESVAR+1)
        tintin_eprintf(ses, "#Invalid message type to toggle: {%s}", type);
    else if (mestype<MAX_MESVAR)
    {
        switch (yes_no(onoff))
        {
        case 0:
            ses->mesvar[mestype]=false;
            break;
        case 1:
            ses->mesvar[mestype]=true;
            break;
        case -1:
            tintin_eprintf(ses, "#messages: Hey! What should I do with %s? Specify a boolean value, not {%s}.",
                    msNAME[mestype], onoff);
            return;
        default:
            ses->mesvar[mestype]=!ses->mesvar[mestype];
        }
        tintin_printf(ses, "#Ok. messages concerning %s are now %s",
                msNAME[mestype], offon[ses->mesvar[mestype]]);
    }
    else
    {
        int b=yes_no(onoff);
        if (b==-1)
        {
            tintin_eprintf(ses, "#messages: Hey! What should I do with all messages? Specify a boolean, not {%s}.", onoff);
            return;
        }
        if (b==-2)
        {
            b=1;
            for (int mestype=0;mestype<MAX_MESVAR;mestype++)
                if (ses->mesvar[mestype])   /* at least one type is ON? */
                    b=0;                    /* disable them all */
        }
        for (int mestype=0;mestype<MAX_MESVAR;mestype++)
            ses->mesvar[mestype]=b;
        if (b)
            tintin_printf(ses, "#Ok. All messages are now ON.");
        else
            tintin_printf(ses, "#Ok. All messages are now OFF.");
    }
}

/**********************/
/* the #snoop command */
/**********************/
void snoop_command(const char *arg, struct session *ses)
{
    if (ses==nullsession)
        return tintin_printf(ses, "#NO SESSION ACTIVE => NO SNOOPING");

    struct session *sesptr = ses;

    char what[BUFFER_SIZE];
    get_arg(arg, what, 1, ses);
    if (*what)
    {
        for (sesptr = sessionlist; sesptr && strcmp(sesptr->name, what); sesptr = sesptr->next) ;
        if (!sesptr)
        {
            tintin_eprintf(ses, "#There is no session named {%s}!", what);
            return;
        }
    }
    if ((sesptr->snoopstatus = !sesptr->snoopstatus))
        tintin_printf(ses, "#SNOOPING SESSION '%s'", sesptr->name);
    else
        tintin_printf(ses, "#UNSNOOPING SESSION '%s'", sesptr->name);
}

/**************************/
/* the #speedwalk command */
/**************************/
void speedwalk_command(const char *arg, struct session *ses)
{
    togglebool(&ses->speedwalk, arg, ses,
               "#SPEEDWALK IS NOW ON.",
               "#SPEEDWALK IS NOW OFF.");
}


/*********************/
/* the #bold command */
/*********************/
void bold_command(const char *arg, struct session *ses)
{
    togglebool(&bold, arg, ses,
               "#Terminals are now allowed to turn bright into bold.",
               "#Terminals are now told to not substitute bright by bold.");
    if (ui_own_output)
    {
        user_pause();
        user_resume();
    }
}


/***********************/
/* the #status command */
/***********************/
void status_command(const char *arg, struct session *ses)
{
    char what[BUFFER_SIZE];

    if (!ui_sep_input)
    {
        tintin_eprintf(ses, "#UI: no managed windows => no status bar");
        return;
    }

    if (ses!=activesession)
        return;
    get_arg(arg, what, 1, ses);
    if (!*what)
        strcpy(what, EMPTY_LINE);
    if (!strcmp(status, what))
        return; /* avoid no-op redraw */
    strlcpy(status, what, BUFFER_SIZE);
    user_show_status();
}


/***********************/
/* the #system command */
/***********************/
void system_command(const char *arg, struct session *ses)
{
    FILE *output;
    char buf[BUFFER_SIZE], ustr[BUFFER_SIZE], what[BUFFER_SIZE];
    mbstate_t cs;
    int save_lastintitle;

    get_arg(arg, what, 1, ses);
    if (!*what)
        return tintin_eprintf(ses, "#EXECUTE WHAT COMMAND?");

    if (ses->mesvar[MSG_SYSTEM])
        tintin_puts1("#EXECUTING SHELL COMMAND.", ses);
    utf8_to_local(buf, what);
    if (!(output = mypopen(buf, false, -1)))
    {
        tintin_puts1("#ERROR EXECUTING SHELL COMMAND.", ses);
        return;
    }
    ZERO(cs);

    save_lastintitle=ses->lastintitle;
    while (fgets(buf, BUFFER_SIZE, output))
    {
        ses->lastintitle=0;
        do_in_MUD_colors(buf, true, ses);
        local_to_utf8(ustr, buf, BUFFER_SIZE, &cs);
        user_textout(ustr);
    }
    ses->lastintitle=save_lastintitle;
    fclose(output);
    if (ses->mesvar[MSG_SYSTEM])
        tintin_printf(ses, "#OK COMMAND EXECUTED.");
}

/**********************/
/* the #shell command */
/**********************/
void shell_command(const char *arg, struct session *ses)
{
    char cmd[BUFFER_SIZE*4], what[BUFFER_SIZE];

    get_arg(arg, what, 1, ses);
    if (!*what)
        return tintin_eprintf(ses, "#EXECUTE WHAT COMMAND?");

    if (ses->mesvar[MSG_SYSTEM])
        tintin_puts1("#EXECUTING SHELL COMMAND.", ses);
    utf8_to_local(cmd, what);
    user_pause();
    if (system(cmd))
    {
         /* yay source hardening retardness -- not only missing /bin/sh is
            illegal on a POSIX system, but also there's no way to check for
            that error without false positives */
    }
    user_resume();
    if (ses->mesvar[MSG_SYSTEM])
        tintin_puts1("#OK COMMAND EXECUTED.", ses);
}


/********************/
/* the #zap command */
/********************/
struct session* zap_command(const char *arg, struct session *ses)
{
    struct session *target = ses;
    char sname[BUFFER_SIZE];

    if (*arg)
    {
        get_arg(arg, sname, 1, ses);
        target = 0;
        for (struct session *s = sessionlist; s; s = s->next)
            if (!strcmp(s->name, sname))
            {
                target = s;
                break;
            }
        if (!target)
        {
            tintin_eprintf(ses, "#THERE'S NO SESSION BY THAT NAME.");
            return ses;
        }
    }

    if (target->closing)
    {
        if (target->closing==-1)
            tintin_eprintf(ses, "#You can't use #ZAP from here.");
        return ses;
    }

    if (target==nullsession)
        end_command("end", (struct session *)NULL); /* no return */

    bool was_active=(target==activesession);
    tintin_printf(target, "#ZZZZZZZAAAAAAAAPPPP!!!!!!!!! LET'S GET OUTTA HERE!!!!!!!!");
    target->closing=1;
    do_hook(target, HOOK_ZAP, 0, true);
    target->closing=0;
    cleanup_session(target);
    return was_active?newactive_session():activesession;
}


#if 0
/*********************************************************************/
/*   tablist will display the all items in the tab completion file   */
/*********************************************************************/
void tablist(struct completenode *tcomplete)
{
    int count, done;
    char tbuf[BUFFER_SIZE];

    done = 0;
    if (!tcomplete)
    {
        tintin_eprintf(0, "Sorry.. But you have no words in your tab completion file");
        return;
    }
    count = 1;
    *tbuf = '\0';

    /*
       I'll search through the entire list, printing three names to a line then
       outputting the line.  Creates a nice 3 column effect.  To increase the #
       if columns, just increase the mod #.  Also.. decrease the # in the %s's
     */

    for (struct completenode *tmp = tcomplete->next; tmp; tmp = tmp->next)
    {
        if ((count % 3))
        {
            if (count == 1)
                sprintf(tbuf, "%25s", tmp->strng);
            else
                sprintf(tbuf, "%s%25s", tbuf, tmp->strng);
            done = 0;
            ++count;
        }
        else
        {
            tintin_printf(0, "%s%25s", tbuf, tmp->strng);
            done = 1;
            *tbuf = '\0';
            ++count;
        }
    }
    if (!done)
        tintin_printf(0, "%s", tbuf);
}

void tab_add(char *arg, struct session *ses)
{
    struct completenode *tmp, *tmpold, *tcomplete;
    struct completenode *newt;
    char *newcomp, buff[BUFFER_SIZE];

    tcomplete = complete_head;

    if (!arg || !strlen(arg))
    {
        tintin_printf(NULL, "Sorry, you must have some word to add.");
        return;
    }
    get_arg(arg, buff, 1, ses);

    if (!(newcomp = (char *)(malloc(strlen(buff) + 1))))
    {
        user_done();
        fprintf(stderr, "Could not allocate enough memory for that Completion word.\n");
        exit(1);
    }
    strcpy(newcomp, buff);
    tmp = tcomplete;
    while (tmp->next)
    {
        tmpold = tmp;
        tmp = tmp->next;
    }

    if (!(newt = (struct completenode *)(malloc(sizeof(struct completenode)))))
    {
        user_done();
        fprintf(stderr, "Could not allocate enough memory for that Completion word.\n");
        exit(1);
    }
    newt->strng = newcomp;
    newt->next = NULL;
    tmp->next = newt;
    tmp = newt;
    tintin_printf(NULL, "#New word %s added to tab completion list.", arg);
}

void tab_delete(char *arg, struct session *ses)
{
    struct completenode *tmp, *tmpold, *tmpnext, *tcomplete;
    char s_buff[BUFFER_SIZE], c_buff[BUFFER_SIZE];

    tcomplete = complete_head;

    if (!arg || !strlen(arg))
    {
        tintin_eprintf(NULL, "#Sorry, you must have some word to delete.");
        return;
    }
    get_arg(arg, s_buff, 1, ses);
    tmp = tcomplete->next;
    tmpold = tcomplete;
    if (!tmpold->strng)
    {                          /* (no list if the second node is null) */
        tintin_eprintf(NULL, "#There are no words for you to delete!");
        return;
    }
    strcpy(c_buff, tmp->strng);
    while (tmp->next && strcmp(c_buff, s_buff))
    {
        tmpold = tmp;
        tmp = tmp->next;
        strcpy(c_buff, tmp->strng);
    }
    if (tmp->next)
    {
        tmpnext = tmp->next;
        tmpold->next = tmpnext;
        free(tmp);
        tintin_printf(NULL, "#Tab word deleted.");
    }
    else
    {
        if (strcmp(c_buff, s_buff) == 0)
        {       /* for the last node to delete */
            tmpold->next = NULL;
            free(tmp);
            tintin_printf(NULL, "#Tab word deleted.");
            return;
        }
        tintin_eprintf(NULL, "Word not found in list.");
    }
}
#endif

/* a human-readable time value */
static void format_time(char *buf, timens_t t)
{
    if (!t)
    {
        buf[0]='0';
        buf[1]=0;
        return;
    }

    bool space = false;
    #define TP(x, suf) if (t/(x)) \
        { buf+=sprintf(buf, space? " %lld%s" : "%lld%s", t/(x), suf); t-=t/(x); space=1; }

    TP(365*24*3600*NANO, "y");
    TP(24*3600*NANO, "d");
    TP(3600*NANO, "h");
    TP(60*NANO, "m");
    if (t % NANO)
        sprintf(buf, "%s%lld.%llds", space? " ":"", t/NANO, (t*10)/NANO%10);
    else
        TP(NANO, "s");
}

void info_command(const char *arg, struct session *ses)
{
    char buffer[BUFFER_SIZE], *bptr, tim1[64], tim2[64];
    int actions   = count_tlist(ses->actions);
    int practions = count_tlist(ses->prompts);
    int aliases   = ses->aliases->nval;
    int subs      = count_tlist(ses->subs);
    int antisubs  = count_slist(ses->antisubs);
    int vars      = ses->myvars->nval;
    int highs     = count_tlist(ses->highs);
    int binds     = ses->binds->nval;
    int pathdirs  = ses->pathdirs->nval;
    int locs = 0;
    for (int i=0;i<ses->num_locations;i++)
        if (ses->locations[i])
            locs++;
    int routes=count_routes(ses);
    switch (ses->sestype)
    {
    case SES_NULL:
        tintin_printf(ses, "Session : {%s}  (null session)", ses->name);
        break;

    case SES_SOCKET:
        tintin_printf(ses, "Session : {%s}  Type: %s  Address : {%s}", ses->name,
#ifdef HAVE_GNUTLS
            ses->ssl?"TCP/IP+SSL" :
#endif
            "TCP/IP", ses->address);
#ifdef HAVE_ZLIB
        tintin_printf(ses, "MCCP compression : %s", ses->mccp?"enabled":"disabled");
#endif
        break;

    case SES_PTY:
        tintin_printf(ses, "Session : {%s}  Type: pty  Command line : {%s}",
            ses->name, ses->address);
        break;

    case SES_SELFPIPE:
        tintin_printf(ses, "Session : {%s}  Type: self-pipe", ses->name);
    }

#ifdef HAVE_SIMD
    tintin_printf(ses, "Using %s pattern matching.", simd? "SIMD" : "non-parallel");
#endif

    tintin_printf(ses, "You have defined the following:");
    tintin_printf(ses, "Actions : %d  Promptactions: %d", actions, practions);
    tintin_printf(ses, "Aliases : %d", aliases);
    tintin_printf(ses, "Substitutes : %d  Antisubstitutes : %d", subs, antisubs);
    tintin_printf(ses, "Variables : %d", vars);
    tintin_printf(ses, "Highlights : %d", highs);
    tintin_printf(ses, "Routes : %d between %d locations", routes, locs);
    tintin_printf(ses, "Binds : %d", binds);
    tintin_printf(ses, "Pathdirs : %d", pathdirs);
    tintin_printf(ses, "Flags: echo=%d, speedwalking=%d, blank=%d, verbatim=%d",
        ses->echo, ses->speedwalk, ses->blank, ses->verbatim);
    tintin_printf(ses, " toggle subs=%d, ignore actions=%d, PreSub=%d, verbose=%d",
        ses->togglesubs, ses->ignore, ses->presub, ses->verbose);
    char num1[32], num2[32];
    usecstr(num1, ses->tick_size);
    usecstr(num2, ses->pretick);
    tintin_printf(ses, "Ticker is %s (ticksize=%s, pretick=%s)",
        ses->tickstatus?"enabled":"disabled", num1, num2);
    if (ui_own_output)
    {
        bptr=buffer;
        if (LINES>1 && COLS>0)
            bptr+=sprintf(bptr, "Terminal size: %dx%d", COLS, LINES);
        else
            bptr+=sprintf(bptr, "Terminal size: unknown");
        if (ui_keyboard)
            bptr+=sprintf(bptr, ", keypad: %s", keypad?"alt mode":"cursor/numeric mode");
        if (ui_sep_input)
            bptr+=sprintf(bptr, ", retain: %d", retain);
        tintin_printf(ses, buffer);
    }
    else
        tintin_printf(ses, "Non-fullscreen mode");
    tintin_printf(ses, "Local charset: %s, remote charset: %s",
        user_charset_name, ses->charset);
    tintin_printf(ses, "Log type: %s, log charset: %s",
        logtypes[ses->logtype], logcs_name(ses->logcharset));
    if (ses->logfile)
        tintin_printf(ses, "Logging to: {%s}", ses->logname);
    else
        tintin_printf(ses, "Not logging");
    if (ses->debuglogfile)
        tintin_printf(ses, "Debuglog: {%s}", ses->debuglogname);
    format_time(tim1, current_time()-start_time);
    tintin_printf(ses, "Session duration: %s", tim1);
    if (ses!=nullsession)
    {
        timens_t ct=current_time();
        format_time(tim1, ct-ses->idle_since);
        format_time(tim2, ct-ses->server_idle_since);
        tintin_printf(ses, "Idle time: %s, server idle: %s", tim1, tim2);
    }
    if (ses->line_time)
        tintin_printf(ses, "Line processing time: %lld.%06llds (%1.1f per second)",
            ses->line_time/NANO, ses->line_time%NANO/1000,
            1/(ses->line_time*0.000000001));
    if (ses->closing)
        tintin_printf(ses, "The session has its closing mark set to %d!", ses->closing);
}

bool isnotblank(const char *line, bool magic_only)
{
    int c;

    if (!strcmp(line, EMPTY_LINE))
        return false;
    if (magic_only)
        return true;
    if (!*line)
        return false;
    while (*line)
        if (*line=='~')
            if (!getcolor(&line, &c, true))
                return true;
            else
                line++;
        else if (isaspace(*line))
            line++;
        else
            return true;
    return false;
}

bool iscompleteprompt(const char *line)
{
    int c=7;
    char ch=' ';

    for (;*line;line++)
        if (*line=='~')
        {
            if (!getcolor(&line, &c, 1))
                ch='~';
        }
        else if (!isaspace(*line))
            ch=*line;
    return strchr("?:>.*$#]&)", ch) && (c==-1||!(c&0x70));
}


/******************************/
/* the #dosubstitutes command */
/******************************/
void dosubstitutes_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left || !*right)
        tintin_eprintf(ses, "#Syntax: #dosubstitutes <var> <text>");
    else
    {
        do_all_sub(right, ses);
        set_variable(left, right, ses);
    }
}

/*****************************/
/* the #dohighlights command */
/*****************************/
void dohighlights_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left || !*right)
        tintin_eprintf(ses, "#Syntax: #dohighlights <var> <text>");
    else
    {
        do_all_high(right, ses);
        set_variable(left, right, ses);
    }
}

/***************************/
/* the #decolorize command */
/***************************/
void decolorize_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], *b;
    int c;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left || !*right)
        tintin_eprintf(ses, "#Syntax: #decolorize <var> <text>");
    else
    {
        b=right;
        for (const char *a=right;*a;a++)
            if (*a=='~')
            {
                if (!getcolor(&a, &c, 1))
                    *b++='~';
            }
            else
                *b++=*a;
        *b=0;
        set_variable(left, right, ses);
    }
}

/*********************/
/* the #atoi command */
/*********************/
void atoi_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], *a;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
        tintin_eprintf(ses, "#Syntax: #atoi <var> <text>");
    else
    {
        if (*(a=right)=='-')
            a++;
        for (; isadigit(*a); a++);
        *a=0;
        if ((a==right+1) && (*right=='-'))
            *right=0;
        set_variable(left, right, ses);
    }
}

/***********************/
/* the #remark command */
/***********************/
void remark_command(const char *arg, struct session *ses)
{
}

/***********************/
/* the #nop(e) command */
/***********************/
/*
 I received a _bug_ report that this command is named "nop" not "nope".
 Even though that's a ridiculous idea, it won't hurt those of us who
 can spell. :p
*/
void nope_command(const char *arg, struct session *ses)
{
}

void else_command(const char *arg, struct session *ses)
{
    tintin_eprintf(ses, "#ELSE WITHOUT IF.");
}

void elif_command(const char *arg, struct session *ses)
{
    tintin_eprintf(ses, "#ELIF WITHOUT IF.");
}

void killall_command(const char *arg, struct session *ses)
{
    kill_all(ses, false);
}

/****************************/
/* the #timecommand command */
/****************************/
void timecommands_command(const char *arg, struct session *ses)
{
    struct timeval tv1, tv2;
    char sec[BUFFER_SIZE], usec[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg(arg, sec, 0, ses);
    arg = get_arg(arg, usec, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*right)
    {
        tintin_eprintf(ses, "#Syntax: #timecommand <sec> <usec> <command>");
        return;
    }
    gettimeofday(&tv1, 0);
    parse_input(right, true, ses);
    gettimeofday(&tv2, 0);
    tv2.tv_sec-=tv1.tv_sec;
    tv2.tv_usec-=tv1.tv_usec;
    if (tv2.tv_usec<0)
    {
        tv2.tv_sec--;
        tv2.tv_usec+=1000000;
    }
    if (*sec || *usec)
    {
        if (*sec)
        {
            sprintf(right, "%d", (int)tv2.tv_sec);
            set_variable(sec, right, ses);
        }
        if (*usec)
        {
            sprintf(right, "%d", (int)tv2.tv_usec);
            set_variable(usec, right, ses);
        }
    }
    else
        tintin_printf(ses, "#Time elapsed: %d.%06d", (int)tv2.tv_sec, (int)tv2.tv_usec);
}

/************************/
/* the #charset command */
/************************/
void charset_command(const char *arg, struct session *ses)
{
    char what[BUFFER_SIZE];
    struct charset_conv nc;

    get_arg(arg, what, 1, ses);

    if (!*what)
    {
        tintin_printf(ses, "#Remote charset: %s", ses->charset);
        return;
    }
    if (!new_conv(&nc, what, 0))
    {
        tintin_eprintf(ses, "#No such charset: {%s}", what);
        return;
    }
    SFREE(ses->charset);
    ses->charset=mystrdup(what);
    if (ses!=nullsession)
    {
        cleanup_conv(&ses->c_io);
        memcpy(&ses->c_io, &nc, sizeof(nc));
    }
    else
        cleanup_conv(&nc);
    tintin_printf(ses, "#Charset set to %s", what);
}


/********************/
/* the #chr command */
/********************/
void chr_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], left[BUFFER_SIZE];
    const char *lp;
    char res[BUFFER_SIZE], *r;
    WC v;

    arg=get_arg(arg, destvar, 0, ses);
    if (!*destvar)
    {
        tintin_eprintf(ses, "#Syntax: #chr <var> <char> [...]");
        return;
    }
    r=res;
    while (*arg)
    {
        arg=get_arg(arg, left, 0, ses);
        lp=left;
        while (*lp)
        {
            v=0;
            if (*lp=='u' || *lp=='U')
            {
                lp++;
                if (*lp=='+')
                    lp++;
            hex:
                switch (*lp)
                {
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    v=v*16 + *lp++-'0';
                    goto hex;
                case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                    v=v*16 + *lp+++10-'a';
                    goto hex;
                case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                    v=v*16 + *lp+++10-'A';
                    goto hex;
                case 0: case ' ': case '\t':
                    break;
                default:
                    tintin_eprintf(ses, "#chr: not a valid hex number in {%s}", left);
                    return;
                }
            }
            else if (lp[0]=='0' && lp[1]=='x')
            {
                lp+=2;
                goto hex;
            }
            else
            {
                while (isadigit(*lp))
                    v=v*10 + *lp++-'0';
                if (*lp && !isaspace(*lp))
                {
                    tintin_eprintf(ses, "#chr: not a valid number in {%s}", left);
                    return;
                }
            }
            if (!v)
            {
                tintin_eprintf(ses, "#chr: can't represent 0 in {%s}", left);
                return;
            }
            if (v>0x10ffff)
            {
                tintin_eprintf(ses, "#chr: not an Unicode value -- got %d=0x%x in {%s}",
                    v, v, left);
                return;
            }
            r+=wc_to_utf8(r, &v, 1, res-r+BUFFER_SIZE);
            lp=space_out(lp);
        }
    }
    *r=0;
    set_variable(destvar, res, ses);
}


/********************/
/* the #ord command */
/********************/
void ord_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], left[BUFFER_SIZE], res[BUFFER_SIZE], *r;
    WC right[BUFFER_SIZE];

    arg=get_arg(arg, destvar, 0, ses);
    if (!*destvar)
    {
        tintin_eprintf(ses, "#Syntax: #ord <var> <string>");
        return;
    }
    r=res;
    get_arg(arg, left, 1, ses);
    utf8_to_wc(right, left, BUFFER_SIZE-1);
    for (WC *cptr=right; *cptr; cptr++)
    {
        if (r-res<BUFFER_SIZE-9)
            r+=sprintf(r, " %u", (unsigned int)*cptr);
        else
        {
            tintin_eprintf(ses, "#ord: result too long");
            goto end;
        }
    }
end:
    if (r==res)
    {
        set_variable(destvar, "", ses);
        return;
    }
    *r=0;
    set_variable(destvar, res+1, ses);
}


/***********************/
/* the #hexord command */
/***********************/
void hexord_command(const char *arg, struct session *ses)
{
    char destvar[BUFFER_SIZE], left[BUFFER_SIZE], res[BUFFER_SIZE], *r;
    WC right[BUFFER_SIZE];

    arg=get_arg(arg, destvar, 0, ses);
    if (!*destvar)
    {
        tintin_eprintf(ses, "#Syntax: #hexord <var> <string>");
        return;
    }
    r=res;
    get_arg(arg, left, 1, ses);
    utf8_to_wc(right, left, BUFFER_SIZE-1);
    for (WC *cptr=right; *cptr; cptr++)
    {
        if (r-res<BUFFER_SIZE-9)
            r+=sprintf(r, " U+%04X", (unsigned int)*cptr);
        else
        {
            tintin_eprintf(ses, "#hexord: result too long");
            goto end;
        }
    }
end:
    if (r==res)
    {
        set_variable(destvar, "", ses);
        return;
    }
    *r=0;
    set_variable(destvar, res+1, ses);
}

/*******************/
/* the #ord inline */
/*******************/
int ord_inline(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];
    WC ch[2];

    get_arg(arg, left, 0, ses);
    if (!*left)
    {
        tintin_eprintf(ses, "#ord: no argument");
        return 0;
    }
    utf8_to_wc(ch, left, 1);
    return ch[0];
}
