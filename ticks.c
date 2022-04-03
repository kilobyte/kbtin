/*********************************************************************/
/* file: ticks.c - functions for the ticker stuff                    */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include <assert.h>
#include "protos/events.h"
#include "protos/globals.h"
#include "protos/hooks.h"
#include "protos/math.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include "protos/vars.h"


/*********************/
/* the #tick command */
/*********************/
void tick_command(const char *arg, struct session *ses)
{
    if (!ses)
        return tintin_puts("#NO SESSION ACTIVE => NO TICKER!", ses);

    char buf[100];
    timens_t to_tick;

    to_tick = ses->tick_size - (current_time() - ses->time0) % ses->tick_size;
    sprintf(buf, "THERE'S NOW %lld.%03d SECONDS TO NEXT TICK.",
        to_tick/NANO, msec(to_tick));
    tintin_puts(buf, ses);
}

/************************/
/* the #tickoff command */
/************************/
void tickoff_command(const char *arg, struct session *ses)
{
    if (!ses)
        return tintin_puts("#NO SESSION ACTIVE => NO TICKER!", ses);

    ses->tickstatus = false;
    if (ses->mesvar[MSG_TICK])
        tintin_puts("#TICKER IS NOW OFF.", ses);
}

/***********************/
/* the #tickon command */
/***********************/
void tickon_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], *err;

    if (!ses)
        return tintin_puts("#NO SESSION ACTIVE => NO TICKER!", ses);

    timens_t ct = current_time();

    get_arg(arg, left, 1, ses);
    substitute_vars(left, left, ses);
    if (*left)
    {
        timens_t x = str2timens(left, &err);
        if (*err || !*left)
            return tintin_eprintf(ses, "#SYNTAX: #tickon [<offset>]");
        if (x < 0)
            return tintin_eprintf(ses, "#NEGATIVE TICK OFFSET");
        ses->time0 = ct - ses->tick_size + x;
    }
    else if (!ses->time0)
        ses->time0 = ct;

    ses->tickstatus = true;
    if (ses->time0 + ses->tick_size - ses->pretick <= ct)
        ses->time10 = ses->time0;
    if (ses->mesvar[MSG_TICK])
        tintin_puts("#TICKER IS NOW ON.", ses);
}


/*************************/
/* the #ticksize command */
/*************************/
void ticksize_command(const char *arg, struct session *ses)
{
    timens_t x;
    char left[BUFFER_SIZE], *err;

    get_arg(arg, left, 1, ses);
    if (!ses)
        return tintin_printf(ses, "#NO SESSION ACTIVE => NO TICKER!");
    if (!*left || !isadigit(*left))
        return tintin_eprintf(ses, "#SYNTAX: #ticksize <number>");
    x=str2timens(left, &err);
    if (*err || x<=0)
        return tintin_eprintf(ses, "#INVALID TICKSIZE");
    ses->tick_size = x;
    ses->time0 = current_time();
    ses->time10 = 0;
    usecstr(left, x);
    if (ses->mesvar[MSG_TICK])
        tintin_printf(ses, "#OK. TICKSIZE SET TO %s", left);
}


/************************/
/* the #pretick command */
/************************/
void pretick_command(const char *arg, struct session *ses)
{
    timens_t x;
    char left[BUFFER_SIZE], *err;

    get_arg(arg, left, 1, ses);
    if (!ses)
        return tintin_printf(ses, "#NO SESSION ACTIVE => NO TICKER!");
    if (!*left)
        x=ses->pretick? 0 : 10 * NANO;
    else
    {
        x=str2timens(left, &err);
        if (*err || x<0)
            return tintin_eprintf(ses, "#INVALID PRETICK DELAY");
    }
    if (x>=ses->tick_size)
    {
        char right[32];
        usecstr(left, x);
        usecstr(right, ses->tick_size);
        tintin_eprintf(ses, "#PRETICK (%s) has to be smaller than #TICKSIZE (%s)",
                left, right);
        return;
    }
    ses->pretick = x;
    if (current_time() - ses->pretick < ses->time0)
        ses->time10 = ses->time0;
    else
        ses->time10 = 0;
    if (!ses->mesvar[MSG_TICK])
        return;
    if (!x)
        tintin_printf(ses, "#OK. PRETICK TURNED OFF");
    else
    {
        usecstr(left, x);
        tintin_printf(ses, "#OK. PRETICK SET TO %s", left);
    }
}


void show_pretick_command(const char *arg, struct session *ses)
{
    pretick_command(arg, ses);
}


timens_t timetilltick(struct session *ses)
{
    return ses->tick_size - (current_time() - ses->time0) % ses->tick_size;
}

/* returns the time (since 1970) of next event (tick) */
timens_t check_event(timens_t time, struct session *ses)
{
    timens_t tt; /* tick time */
    timens_t et; /* event time */
    struct eventnode *ev;

    assert(ses);

    /* events check  - that should be done in #delay */
    while ((ev=ses->events) && (ev->time<=time))
    {
        ses->events=ev->next;
        execute_event(ev, ses);
        TFREE(ev, struct eventnode);
        if (any_closed)
            return -1;
    }
    et = (ses->events) ? ses->events->time : 0;

    /* ticks check */
    tt = ses->time0 + ses->tick_size; /* expected time of tick */

    if (tt <= time)
    {
        if (ses->tickstatus)
        {
            if (do_hook(ses, HOOK_TICK, 0, false) == ses
                && ses->mesvar[MSG_TICK])
            {
                tintin_puts1("#TICK!!!", ses);
            }
        }
        if (any_closed)
            return -1;
        ses->time0 = time - (time - ses->time0) % ses->tick_size;
        tt = ses->time0 + ses->tick_size;
    }
    else if (ses->tickstatus && tt-ses->pretick<=time
            && ses->tick_size>ses->pretick && ses->time10<ses->time0)
    {
        if (do_hook(ses, HOOK_PRETICK, 0, false) == ses
            && ses->mesvar[MSG_TICK])
        {
            char buf[BUFFER_SIZE], num[32];
            usecstr(num, ses->pretick);
            sprintf(buf, "#%s SECONDS TO TICK!!!", num);
            tintin_puts1(buf, ses);
        }
        if (any_closed)
            return -1;
        ses->time10 = ses->time0;
    }

    if (ses->tickstatus && ses->tick_size>ses->pretick && tt-time>ses->pretick)
        tt-=ses->pretick;

    return (et<tt && et) ? et : tt;
}
