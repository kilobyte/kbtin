#include "tintin.h"
#include <inttypes.h>
#include "protos/alias.h"
#include "protos/hash.h"
#include "protos/math.h"
#include "protos/parse.h"
#include "protos/print.h"
#include "protos/string.h"
#include "protos/utils.h"

struct session *ratelimit_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], command[BUFFER_SIZE], *old;
    arg = get_arg(arg, left, 0, ses);
    arg = get_arg(arg, right, 0, ses);
    arg = get_arg(arg, command, 0, ses);

    if (!*left)
    {
        tintin_eprintf(ses, "#Syntax: #ratelimit {label} {3 10s} {burp} #else ...");
        return ses;
    }

    timens_t now = current_time();
    timens_t tip = 0, period = 0;
    num_t limit = 0;

    if ((old=get_hash(ses->ratelimits, left)))
    {
        if (sscanf(old, "%" SCNd64" %" SCNd64" %" SCNd64, &tip, &limit, &period)!=3)
            tintin_eprintf(ses, "#Internal error: ratelimit accounting mangled");
        // try to continue
    }

    // tip is the moment the rate bucket will become empty
    if (tip < now)
        tip = now; // it's already empty, yay!

#if 0
    if (!*right)
    {
        show_hashlist(ses, ses->ratelimits, left,
            "#THESE RATELIMITS HAVE BEEN SET:",
            "#THAT RATELIMIT IS NOT DEFINED.");
        return ses;
    }
#endif

    if (*right)
    {
        const char *rd;
        limit = str2num(right, (char**)&rd);
        rd = space_out(rd);
        if (!*rd)
        {
            tintin_eprintf(ses, "#Error: ratelimit specification needs to be {count period}, got {%s}", right);
            return ses;
        }
        period = time2secs(rd, ses);
        if (period <= 0)
        {
            tintin_eprintf(ses, "#Error: bad ratelimit specification: {%s}", right);
            return ses;
        }
    }
    else
    {
        // reuse old settings
        if (period <= 0)
        {
            tintin_eprintf(ses, "#No recorded ratelimit specification for {%s}, define it please!", left);
            return ses;
        }
    }

    if (!*command)
    {
        sprintf(right, "%" PRId64" %" PRId64" %" PRId64, tip, limit, period);
        set_hash(ses->ratelimits, left, right);

        if (ses->mesvar[MSG_RATELIMIT])
            tintin_printf(ses, "#Defined a ratelimit for %s", left);
        return ses;
    }

    timens_t cost = 0;
    if (limit >= DENOM) // limit < 1.0 disallows any work
        cost = ndiv(period, limit);

    if (cost && tip - now + cost <= period)
    {
        tip += cost;
        sprintf(right, "%" PRId64" %" PRId64" %" PRId64, tip, limit, period);
        set_hash(ses->ratelimits, left, right);

        return parse_input(command, true, ses);
    }

    if (ses->mesvar[MSG_RATELIMIT])
    {
        if (cost)
            tintin_printf(ses, "#Ratelimit for {%s} exceeded", left);
        else
            tintin_printf(ses, "#Ratelimit for {%s} allows nothing", left);
    }

    return ifelse("ratelimit", arg, ses);
}

/****************************/
/* the #unratelimit command */
/****************************/
void unratelimit_command(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE];

    do
    {
        arg = get_arg(arg, left, 0, ses);
        delete_hashlist(ses, ses->ratelimits, left,
            ses->mesvar[MSG_RATELIMIT]? "#Ok. Cleared ratelimit for %s." : 0,
            ses->mesvar[MSG_RATELIMIT]? "#THAT RATELIMIT (%s) IS NOT DEFINED." : 0);
    } while (*arg);
}
