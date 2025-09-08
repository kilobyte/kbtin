#include "tintin.h"
#include "protos/action.h"
#include "protos/antisub.h"
#include "protos/globals.h"
#include "protos/highlight.h"
#include "protos/print.h"
#include "protos/misc.h"
#include "protos/substitute.h"
#include "protos/user.h"

bool puts_echoing = true;

/****************************************************/
/* output to screen should go through this function */
/* text gets checked for substitutes and actions    */
/****************************************************/
void tintin_puts1(const char *cptr, session *ses)
{
    char line[BUFFER_SIZE];
    stracpy(line, cptr, sizeof line);

    _=line;
    if (!ses->presub && !ses->ignore)
        check_all_actions(line, ses);
    if (!ses->togglesubs)
        if (!do_one_antisub(line, ses))
            do_all_sub(line, ses);
    if (ses->presub && !ses->ignore)
        check_all_actions(line, ses);
    if (!ses->togglesubs)
        do_all_high(line, ses);
    if (isnotblank(line, ses->blank))
        if (ses==activesession)
        {
            char *cp=strchr(line, 0);
            if (cp-line>=BUFFER_SIZE-2)
                cp=line+BUFFER_SIZE-2;
            cp[0]='\n';
            cp[1]=0;
            user_textout(line);
        }
    _=0;
}

// In these two functions, ses=0 means a message whose context is not
// attached to a specific session, not even the nullsession.

void tintin_printf(session *ses, const char *format, ...)
{
    va_list ap;
    char buf[BUFFER_SIZE];

    if ((ses != activesession && ses != nullsession && ses) || !puts_echoing)
        return;

    va_start(ap, format);
    int n=vsnprintf(buf, BUFFER_SIZE-1, format, ap);
    if (n>BUFFER_SIZE-2)
    {
        buf[BUFFER_SIZE-3]='>';
        n=BUFFER_SIZE-2;
    }
    va_end(ap);
    strcpy(buf+n, "\n");
    user_textout(buf);
}

void tintin_printf(msg_t msgt, session *ses, const char *format, ...)
{
    va_list ap;
    char buf[BUFFER_SIZE];

    if (!ses->mesvar[msgt])
        return;
    if ((ses != activesession && ses != nullsession && ses) || !puts_echoing)
        return;

    va_start(ap, format);
    int n=vsnprintf(buf, BUFFER_SIZE-1, format, ap);
    if (n>BUFFER_SIZE-2)
    {
        buf[BUFFER_SIZE-3]='>';
        n=BUFFER_SIZE-2;
    }
    va_end(ap);
    strcpy(buf+n, "\n");
    user_textout(buf);
}

void tintin_eprintf(session *ses, const char *format, ...)
{
    va_list ap;
    char buf[BUFFER_SIZE];

    if (ses && ses != activesession)
        return; // we're not foreground
    if (!puts_echoing && ses && !ses->mesvar[MSG_ERROR])
        return; // disabled and not in "show errors" mode

    va_start(ap, format);
    int n=vsnprintf(buf, BUFFER_SIZE-1, format, ap);
    if (n>BUFFER_SIZE-2)
    {
        buf[BUFFER_SIZE-3]='>';
        n=BUFFER_SIZE-2;
    }
    va_end(ap);
    strcpy(buf+n, "\n");

    char out[BUFFER_SIZE];
    char *cptr=buf, *optr=out;

    while (*cptr)
    {
        if ((*optr++=*cptr++)=='~')
            *optr++='~', *optr++=':', *optr++='~';
        if (optr-out > BUFFER_SIZE-1)
            break;
    }
    *optr=0;

    user_textout(out);
}
