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

static void schedule_event(struct session *ses, struct eventnode *restrict ev)
{
    // TODO: burn this code and replace without O(n)
    struct eventnode *ptr, *ptrlast;

    if (!ses->events)
    {
        ses->events = ev;
        ev->next = NULL;
    }
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

static void execute_event(struct eventnode *ev, struct session *ses)
{
    if (activesession==ses && ses->mesvar[MSG_EVENT])
        tintin_printf(ses, "[EVENT: %s]", ev->event);
    parse_input(ev->event, true, ses);
    recursion=0;
}

/* execute due events, abort and return 1 if any_closed */
bool do_events(struct session *ses, timens_t now)
{
    struct eventnode *ev;

    while ((ev=ses->events) && (ev->time<=now))
    {
        ses->events=ev->next;
        execute_event(ev, ses);

        if (ev->period >= 0)
        {
            // If badly lagged, wait at least half of period.
            timens_t old = ev->time - now;
            timens_t del = ev->period;
            timens_t new = old + del;
            timens_t cap = del/2;
            if (new < cap)
                new = cap;
            if (new <= 0)
                new = 1; // rapid-fire events shouldn't starve others
            ev->time = now + new;
            schedule_event(ses, ev);
        }
        else
        {
            SFREE(ev->event);
            TFREE(ev, struct eventnode);
        }
        if (any_closed)
            return 1;
    }

    return 0;
}

static void show_event(struct session *ses, struct eventnode *ev, timens_t ct)
{
    if (ev->period >= 0)
        tintin_printf(ses, "(%lld.%03d, %lld.%03d)\t {%s}",
            (ev->time-ct)/NANO, msec(ev->time-ct),
            (ev->period)/NANO, msec(ev->period),
            ev->event);
    else
        tintin_printf(ses, "(%lld.%03d)\t\t {%s}", (ev->time-ct)/NANO,
            msec(ev->time-ct), ev->event);
}

/* list active events matching regexp arg */
static void list_events(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];
    timens_t ct = current_time();
    struct eventnode *ev = ses->events;

    arg = get_arg_in_braces(arg, left, 1);

    if (!*left)
    {
        tintin_printf(ses, "#Defined events:");
        while (ev)
        {
            show_event(ses, ev, ct);
            ev = ev->next;
        }
        return;
    }

    bool flag = false;
    while (ev)
    {
        if (match(left, ev->event))
        {
            show_event(ses, ev, ct);
            flag = true;
        }
        ev = ev->next;
    }
    if (!flag)
        tintin_printf(ses, "#THAT EVENT (%s) IS NOT DEFINED.", left);
}

/* add new event to the list */
void delay_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];
    timens_t period = -1;

    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 1, ses);
    if (!*right)
        return list_events(left, ses);

    if (!*left)
        return tintin_eprintf(ses, "#EVENT IGNORED, NO DELAY GIVEN");

    char *slash = strchr(left, ',');
    if (slash)
        *slash = 0;

    timens_t delay = time2secs(left, ses);
    if (delay==INVALID_TIME || delay<0)
        return tintin_eprintf(ses, "#EVENT IGNORED (DELAY={%s}), INVALID DELAY", left);

    if (slash)
    {
        slash++;
        period = time2secs(slash, ses);
        if (period==INVALID_TIME || period<0)
            return tintin_eprintf(ses, "#EVENT IGNORED (PERIOD={%s}), INVALID PERIOD", slash);
    }

    struct eventnode *ev = TALLOC(struct eventnode);
    ev->time = current_time() + delay;
    ev->period = period;
    ev->event = mystrdup(right);

    schedule_event(ses, ev);
}

void event_command(const char *arg, struct session *ses)
{
    delay_command(arg, ses);
}

/* remove ev->next from list */
static void remove_event(struct eventnode **ev)
{
    struct eventnode *tmp = (*ev)->next;
    SFREE((*ev)->event);
    TFREE(*ev, struct eventnode);
    *ev=tmp;
}

/* remove events matching regexp arg from list */
void undelay_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    arg = get_arg(arg, left, 1, ses);

    if (!*left)
        return tintin_eprintf(ses, "#ERROR: valid syntax is: #undelay {event pattern}");

    timens_t ct = current_time();

    bool flag = false;
    struct eventnode **ev = &(ses->events);
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
