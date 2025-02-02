/* Do all the telnet protocol stuff */

#include "tintin.h"
#include "protos/globals.h"
#include "protos/print.h"
#include "protos/run.h"
#include "protos/net.h"
#include "protos/utils.h"


#define EOR 239     /* End of Record */
#define SE  240     /* subnegotiation end */
#define NOP 241     /* no operation */
#define DM  242     /* Data Mark */
#define BRK 243     /* Break */
#define IP  244     /* interrupt process */
#define AO  245     /* abort output */
#define AYT 246     /* Are you there? */
#define EC  247     /* erase character */
#define EL  248     /* erase line */
#define GA  249     /* go ahead */
#define SB  250     /* subnegotiations */
#define WILL    251
#define WONT    252
#define DO      253
#define DONT    254
#define IAC 255     /* interpret as command */

#undef ECHO

#define ECHO                1
#define SUPPRESS_GO_AHEAD   3
#define STATUS              5
#define TERMINAL_TYPE       24
#define END_OF_RECORD       25
#define NAWS                31
#define COMPRESS1           85
#define COMPRESS2           86

#define IS      0
#define SEND    1

/* sanity check */
#define MAX_SUBNEGO_LENGTH 64

#ifdef TELNET_DEBUG
static const char *will_names[4]={"WILL", "WONT", "DO", "DONT"};
static const char *option_names[]=
    {
        "Binary Transmission",
        "Echo",
        "Reconnection",
        "Suppress Go Ahead",
        "Approx Message Size Negotiation",
        "Status",
        "Timing Mark",
        "Remote Controlled Trans and Echo",
        "Output Line Width",
        "Output Page Size",
        "Output Carriage-Return Disposition",
        "Output Horizontal Tab Stops",
        "Output Horizontal Tab Disposition",
        "Output Formfeed Disposition",
        "Output Vertical Tabstops",
        "Output Vertical Tab Disposition",
        "Output Linefeed Disposition",
        "Extended ASCII",
        "Logout",
        "Byte Macro",
        "Data Entry Terminal",
        "SUPDUP",
        "SUPDUP Output",
        "Send Location",
        "Terminal Type",
        "End of Record",
        "TACACS User Identification",
        "Output Marking",
        "Terminal Location Number",
        "Telnet 3270 Regime",
        "X.3 PAD",
        "Negotiate About Window Size",
        "Terminal Speed",
        "Remote Flow Control",
        "Linemode",
        "X Display Location",
        "Environment Option",
        "Authentication Option",
        "Encryption Option",
        "New Environment Option",
        "TN3270E",
        "XAUTH",
        "CHARSET",
        "Telnet Remote Serial Port (RSP)",
        "Com Port Control Option",
        "Telnet Suppress Local Echo",
        "Telnet Start TLS",
        "KERMIT",
        "SEND-URL",
        "FORWARD_X",
        "?"/*50*/,"?","?","?","?","?","?","?","?","?",
        "?"/*60*/,"?","?","?","?","?","?","?","?","?",
        "?"/*70*/,"?","?","?","?","?","?","?","?","?",
        "?"/*80*/,"?","?","?","?",
        "COMPRESS",
        "COMPRESS2",
    };
#endif

static void telnet_send_naws(struct session *ses)
{
    unsigned char nego[128], *np;

#define PUTBYTE(b)    if ((b)==255) *np++=255;   *np++=(b);
    np=nego;
    *np++=IAC;
    *np++=SB;
    *np++=NAWS;
    PUTBYTE(COLS/256);
    PUTBYTE(COLS%256);
    PUTBYTE((LINES-1-!!isstatus)/256);
    PUTBYTE((LINES-1-!!isstatus)%256);
    *np++=IAC;
    *np++=SE;
    write_socket(ses, (char*)nego, np-nego);
#ifdef TELNET_DEBUG
    {
        char buf[BUFFER_SIZE], *b=buf;
        int neb=np-nego-2;
        np=nego+3;
        b=buf+sprintf(buf, "IAC SB NAWS ");
        while (np-nego<neb)
            b+=sprintf(b, "<%u> ", *np++);
        b+=sprintf(b, "IAC SE");
        tintin_printf(ses, "~8~[telnet] sent: %s~-1~", buf);
    }
#endif
}

static void telnet_send_ttype(struct session *ses)
{
    char nego[128];
    const char *ttype;

    switch (ses->last_term_type++)
    {
    case 0:
        ttype=TERM;
        break;
    case 1:
        ttype="hardcopy";
        break;
    case 2:
        ttype="unknown";
        break;
    /* contrary to what zMud does, we cannot claim we're "vt100" or "ansi", */
    /* as we obviously lack an addressable cursor */
    default:
        ses->last_term_type=0;
    case 3:
        ttype="KBtin-"VERSION;
    }
    write_socket(ses, nego,
        sprintf(nego, "%c%c%c%c%s%c%c", IAC, SB,
            TERMINAL_TYPE, IS, ttype, IAC, SE));
#ifdef TELNET_DEBUG
    tintin_printf(ses, "~8~[telnet] sent: IAC SB TERMINAL-TYPE IS \"%s\" IAC SE~-1~", ttype);
#endif
}

void telnet_resize_all(void)
{
    for (struct session *sp=sessionlist; sp; sp=sp->next)
        if (sp->naws)
            switch (sp->sestype)
            {
            case SES_SOCKET:
                telnet_send_naws(sp);
                break;
            case SES_PTY:
                pty_resize(sp->socket, COLS, LINES-1-!!isstatus);
                break;
            default:;
            }
}

int do_telnet_protocol(const char *data, int nb, struct session *ses)
{
    const unsigned char *cp = (const unsigned char*)data+1;
    unsigned char wt;
    unsigned char answer[3];
    unsigned char nego[128], *np;

#define NEXTCH  cp++;                               \
                if (cp-(unsigned char*)data>=nb)    \
                    return -1;

    if (nb<2)
        return -1;
    switch (*cp)
    {
    case WILL:
    case WONT:
    case DO:
    case DONT:
        if (nb<3)
            return -1;
        wt=*(cp++);
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] received: IAC %s <%u> (%s)~-1~",
                      will_names[wt-251], *cp,
                      (*cp<sizeof(option_names)/sizeof(char*))?
                          option_names[*cp]:"?");
#endif
        answer[0]=IAC;
        answer[2]=*cp;
        switch (*cp)
        {
        case ECHO:
            switch (wt)
            {
            case WILL:  answer[1]=DO;   ses->server_echo=1; break;
            case DO:    answer[1]=WONT; break;
            case WONT:  answer[1]=DONT; ses->server_echo=2; break;
            case DONT:  answer[1]=WONT; break;
            }
            break;
        case TERMINAL_TYPE:
            switch (wt)
            {
            case WILL:  answer[1]=DONT; break;
            case DO:    answer[1]=WILL; break;
            case WONT:  answer[1]=DONT; break;
            case DONT:  answer[1]=WONT; break;
            }
            break;
        case NAWS:
            switch (wt)
            {
            case WILL:  answer[1]=DO;   ses->naws=false; break;
            case DO:    answer[1]=WILL; ses->naws=(LINES>1 && COLS>0); break;
            case WONT:  answer[1]=DONT; ses->naws=false; break;
            case DONT:  answer[1]=WONT; ses->naws=false; break;
            }
            break;
        case END_OF_RECORD:
            switch (wt)
            {
            case WILL:  answer[1]=DO;   break;
            case DO:    answer[1]=WONT; break;
            case WONT:  answer[1]=DONT; break;
            case DONT:  answer[1]=WONT; break;
            }
            break;
#ifdef HAVE_ZLIB
        case COMPRESS2:
            switch (wt)
            {
            case WILL:  answer[1]=DO;   ses->can_mccp=current_time()-ses->sessionstart<60*NANO; break;
            case DO:    answer[1]=WONT; break;
            case WONT:  answer[1]=DONT; ses->can_mccp=false; break;
            case DONT:  answer[1]=WONT; break;
            }
            break;
#endif
        default:
            switch (wt)
            {
            case WILL:  answer[1]=DONT; break;
            case DO:    answer[1]=WONT; break;
            case WONT:  answer[1]=DONT; break;
            case DONT:  answer[1]=WONT; break;
            }
        }
        write_socket(ses, (char*)answer, 3);
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] sent: IAC %s <%u> (%s)~-1~",
                      will_names[answer[1]-251], *cp,
                      (*cp<sizeof(option_names)/sizeof(char*))?
                          option_names[*cp]:"?");
#endif
        if (*cp==NAWS)
            telnet_send_naws(ses);
        return 3;
    case SB:
        np=nego;
sbloop:
        NEXTCH;
        while (*cp!=IAC)
        {
            if (np-nego>MAX_SUBNEGO_LENGTH)
                goto nego_too_long;
            *np++=*cp;
            NEXTCH;
        }
        NEXTCH;
        if (*cp==IAC)
        {
            if (np-nego>MAX_SUBNEGO_LENGTH)
                goto nego_too_long;
            *np++=IAC;
            goto sbloop;
        }
        if (*cp!=SE)
        {
            if (np-nego>MAX_SUBNEGO_LENGTH)
                goto nego_too_long;
            *np++=*cp;
            goto sbloop;
        }
        nb=cp-(unsigned char*)data;
#ifdef TELNET_DEBUG
        {
            char buf[BUFFER_SIZE], *b=buf;
            unsigned int neb=np-nego;
            np=nego;
            b=buf+sprintf(buf, "IAC SB ");
            switch (*np)
            {
            case TERMINAL_TYPE:
                b+=sprintf(b, "TERMINAL-TYPE ");
                if (*++np==SEND)
                {
                    b+=sprintf(b, "SEND ");
                    ++np;
                }
                break;
            case COMPRESS2:
                b+=sprintf(b, "COMPRESS2 ");
                np++;
            }
            while (np-nego<neb)
                b+=sprintf(b, "<%u> ", *np++);
            b+=sprintf(b, "IAC SE");
            tintin_printf(ses, "~8~[telnet] received: %s~-1~", buf);
        }
#endif
        switch (*(np=nego))
        {
        case TERMINAL_TYPE:
            if (*(np+1)==SEND)
                telnet_send_ttype(ses);
            break;
#ifdef HAVE_ZLIB
        case COMPRESS2:
            if (ses->can_mccp)
                return -4; /* compressed data immediately follows, we need to return */
#endif
        }
        return nb+1;
    case GA:
    case EOR:
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] received: IAC %s~-1~",
            (*cp==GA)?"GA":"EOR");
#endif
        ses->gas=true;
        return -2;
    case IAC:       /* IAC IAC is the escape for literal 255 byte */
        return -3;
    default:
        /* other 2-byte command, ignore */
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] received: IAC <%u>~-1~", *cp&255);
#endif
        return 2;
    }
    /* not reached */
    return cp-(unsigned char*)data;
nego_too_long:
    tintin_eprintf(ses, "#error: unterminated TELNET subnegotiation received.");
    return 2; /* we leave everything but IAC SB */
}

void telnet_write_line(const char *line, struct session *ses, bool nl)
{
    char outtext[6*BUFFER_SIZE + 2], *out;

    out=outtext;
    while (*line)
    {
        if ((unsigned char)*line==255)
            *out++=(char)255;
        *out++=*line++;
    }
    if (nl)
        *out++='\r', *out++='\n';
    *out=0;

    write_socket(ses, outtext, out-outtext);
}
