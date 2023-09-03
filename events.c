#include "tintin.h"
#include "protos/action.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/lists.h"
#include "protos/math.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/vars.h"


int msec(timens_t t)
{
    t%=NANO;
    if (t<0)
        t+=NANO;
    return t/1000000;
}

void execute_event(struct eventnode *ev, struct session *ses)
{
    if (activesession==ses && ses->mesvar[MSG_EVENT])
        tintin_printf(ses, "[EVENT: %s]", ev->event);
    parse_input(ev->event, true, ses);
    recursion=0;
}

/* list active events matching regexp arg */
static void list_events(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];
    timens_t ct; /* current time */
    bool flag;
    struct eventnode *ev;

    ct = current_time();
    ev = ses->events;
    arg = get_arg_in_braces(arg, left, 1);

    if (!*left)
    {
        tintin_printf(ses, "#Defined events:");
        while (ev)
        {
            tintin_printf(ses, "(%lld.%03d)\t {%s}", (ev->time-ct)/NANO,
                msec(ev->time-ct), ev->event);
            ev = ev->next;
        }
    }
    else
    {
        flag = false;
        while (ev)
        {
            if (match(left, ev->event))
            {
                tintin_printf(ses, "(%lld.%03d)\t {%s}", (ev->time-ct)/NANO,
                    msec(ev->time-ct), ev->event);
                flag = true;
            }
            ev = ev->next;
        }
        if (!flag)
            tintin_printf(ses, "#THAT EVENT (%s) IS NOT DEFINED.", left);
    }
}

/* add new event to the list */
void delay_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], *cptr;
    timens_t delay;
    struct eventnode *ev, *ptr, *ptrlast;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*right)
    {
        list_events(left, ses);
        return;
    }

    if (!*left || (delay=str2timens(left, &cptr))<0 || *cptr)
        return tintin_eprintf(ses, "#EVENT IGNORED (DELAY={%s}), NEGATIVE DELAY", left);

    ev = TALLOC(struct eventnode);
    ev->time = current_time() + delay;
    ev->next = NULL;
    ev->event = mystrdup(right);

    if (!ses->events)
        ses->events = ev;
    else if (ses->events->time > ev->time)
    {
        ev->next = ses->events;
        ses->events = ev;
    }
    else
    {
        ptr = ses->events;
        while ((ptrlast = ptr) && (ptr = ptr->next))
        {
            if (ptr->time > ev->time)
            {
                ev->next = ptr;
                ptrlast->next = ev;
                return;
            }
        }
        ptrlast->next = ev;
        ev->next = NULL;
    }
}

void event_command(const char *arg, struct session *ses)
{
    delay_command(arg, ses);
}

/* remove ev->next from list */
static void remove_event(struct eventnode **ev)
{
    struct eventnode *tmp;
    if (*ev)
    {
        tmp = (*ev)->next;
        SFREE((*ev)->event);
        TFREE(*ev, struct eventnode);
        *ev=tmp;
    }
}

/* remove events matching regexp arg from list */
void undelay_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];
    bool flag;
    struct eventnode **ev;

    arg = get_arg(arg, left, 1, ses);

    if (!*left)
        return tintin_eprintf(ses, "#ERROR: valid syntax is: #undelay {event pattern}");

    timens_t ct = current_time();

    flag = false;
    ev = &(ses->events);
    while (*ev)
        if (match(left, (*ev)->event))
        {
            flag=true;
            if (ses==activesession && ses->mesvar[MSG_EVENT])
                tintin_printf(ses, "#Ok. Event {%s} at %lld.%03d won't be executed.",
                    (*ev)->event, ((*ev)->time-ct)/NANO, msec((*ev)->time-ct));
            remove_event(ev);
        }
        else
            ev=&((*ev)->next);

    if (!flag)
        tintin_printf(ses, "#THAT EVENT IS NOT DEFINED.");
}

void removeevent_command(const char *arg, struct session *ses)
{
    undelay_command(arg, ses);
}

void unevent_command(const char *arg, struct session *ses)
{
    undelay_command(arg, ses);
}

void kill_events(struct session *ses)
{
    struct eventnode *ev = ses->events;
    while (ev)
        remove_event(&ev);
    ses->events = 0;
}

void findevents_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*left)
    {
        tintin_eprintf(ses, "#Syntax: #findevents <result> <pattern>");
        return;
    }

    if (!*right)
        strcpy(right, "*");

    timens_t ct = current_time();

    char buf[BUFFER_SIZE], *b=buf;
    *buf=0;
    struct eventnode *ev = ses->events;
    while (ev)
    {
        if (match(right, ev->event))
        {
            char time[BUFFER_SIZE], this[BUFFER_SIZE+10];
            // lie that overdue events are due right now
            nsecstr(time, (ev->time>ct)? ev->time-ct : 0);
            if (isatom(ev->event))
                snprintf(this, sizeof this, "{%s %s}", time, ev->event);
            else
                snprintf(this, sizeof this, "{%s {%s}}", time, ev->event);

            if (b!=buf)
                *b++=' ';
            b+=snprintf(b, buf-b+sizeof(buf), "%s", this);
            if (b >= buf+BUFFER_SIZE-10)
            {
                tintin_eprintf(ses, "#Too many events to store.");
                break;
            }
        }
        ev = ev->next;
    }

    set_variable(left, buf, ses);
}
