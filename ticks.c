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
#include "protos/print.h"
#include "protos/parse.h"

time_t time0;
int utime0;

/*********************/
/* the #tick command */
/*********************/
void tick_command(const char *arg, struct session *ses)
{
    if (ses)
    {
        char buf[100];
        timens_t to_tick;

        to_tick = ses->tick_size - (time(NULL) - ses->time0) % ses->tick_size;
        sprintf(buf, "THERE'S NOW %d SECONDS TO NEXT TICK.", to_tick/NANO);
        tintin_puts(buf, ses);
    }
    else
        tintin_puts("#NO SESSION ACTIVE => NO TICKER!", ses);
}

/************************/
/* the #tickoff command */
/************************/
void tickoff_command(const char *arg, struct session *ses)
{
    if (ses)
    {
        ses->tickstatus = false;
        tintin_puts("#TICKER IS NOW OFF.", ses);
    }
    else
        tintin_puts("#NO SESSION ACTIVE => NO TICKER!", ses);
}

/***********************/
/* the #tickon command */
/***********************/
void tickon_command(const char *arg, struct session *ses)
{
    if (ses)
    {
        ses->tickstatus = true;
        timens_t ct = current_time();
        if (ses->time0 == 0)
            ses->time0 = ct;
        if (ses->time0 + ses->tick_size - ses->pretick <= ct)
            ses->time10 = ses->time0;
        tintin_puts("#TICKER IS NOW ON.", ses);
    }
    else
        tintin_puts("#NO SESSION ACTIVE => NO TICKER!", ses);
}


/*************************/
/* the #ticksize command */
/*************************/
void ticksize_command(const char *arg, struct session *ses)
{
    int x;
    char left[BUFFER_SIZE], *err;

    get_arg(arg, left, 1, ses);
    if (!ses)
    {
        tintin_printf(ses, "#NO SESSION ACTIVE => NO TICKER!");
        return;
    }
    if (!*left || !isadigit(*left))
    {
        tintin_eprintf(ses, "#SYNTAX: #ticksize <number>");
        return;
    }
    x=strtol(left, &err, 10);
    if (*err || x<1 || x>=0x7fffffff)
    {
        tintin_eprintf(ses, "#TICKSIZE OUT OF RANGE (1..%d)", 0x7fffffff);
        return;
    }
    ses->tick_size = x*NANO;
    ses->time0 = current_time();
    ses->time10 = 0;
    tintin_printf(ses, "#OK. NEW TICKSIZE SET");
}


/************************/
/* the #pretick command */
/************************/
void pretick_command(const char *arg, struct session *ses)
{
    int x;
    char left[BUFFER_SIZE], *err;

    get_arg(arg, left, 1, ses);
    if (!ses)
    {
        tintin_printf(ses, "#NO SESSION ACTIVE => NO TICKER!");
        return;
    }
    if (!*left)
        x=ses->pretick? 0 : 10;
    else
    {
        x=strtol(left, &err, 10);
        if (*err || x<0 || x>=0x7fffffff)
        {
            tintin_eprintf(ses, "#PRETICK VALUE OUT OF RANGE (0..%d)", 0x7fffffff);
            return;
        }
    }
    if (x>=ses->tick_size)
    {
        tintin_eprintf(ses, "#PRETICK (%d) has to be smaller than #TICKSIZE (%d)",
            x, ses->tick_size/NANO);
        return;
    }
    ses->pretick = x*NANO;
    if (current_time() - ses->pretick < ses->time0)
        ses->time10 = ses->time0;
    else
        ses->time10 = 0;
    if (x)
        tintin_printf(ses, "#OK. PRETICK SET TO %d", x);
    else
        tintin_printf(ses, "#OK. PRETICK TURNED OFF");
}


void show_pretick_command(const char *arg, struct session *ses)
{
    pretick_command(arg, ses);
}


int timetilltick(struct session *ses)
{
    return (ses->tick_size - (current_time() - ses->time0) % ses->tick_size)/NANO;
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
            if (do_hook(ses, HOOK_TICK, 0, false) == ses)
                tintin_puts1("#TICK!!!", ses);
        }
        if (any_closed)
            return -1;
        ses->time0 = time - (time - ses->time0) % ses->tick_size;
        tt = ses->time0 + ses->tick_size;
    }
    else if (ses->tickstatus && tt-ses->pretick<=time
            && ses->tick_size>ses->pretick && ses->time10<ses->time0)
    {
        if (do_hook(ses, HOOK_PRETICK, 0, false) == ses)
        {
            char buf[BUFFER_SIZE];
            sprintf(buf, "#%d SECONDS TO TICK!!!", ses->pretick/NANO);
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
