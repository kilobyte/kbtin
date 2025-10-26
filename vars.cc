#include "tintin.h"
#include <time.h>
#include "protos/globals.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include "protos/hash.h"
#include "protos/math.h"
#include "protos/path.h"
#include "protos/session.h"
#include "protos/ticks.h"


/*****************************************/
/* delete ; and unpaired/underflowing {} */
/*****************************************/
static int defang_var(char *result, const char *var)
{
    int obraces=0, level=0;

    for (const char *p=var; *p; p++)
    {
        if (*p == '{')
            obraces++, level++;
        else if (*p == '}')
            if (level>0)
                level--;
    }

    obraces -= level; /* # of unpaired */
    level = 0;

    char *r = result;
    for (const char *p=var; *p; p++)
    {
        if (*p == ';')
            ; /* no replacement */
        else if (*p == '{')
        {
            *r++ = (obraces-->0)? '{' : '(';
            level++;
        }
        else if (*p == '}')
        {
            if (level > 0)
                *r++ = '}', level--;
            else
                *r++ = ')';
        }
        else
            *r++ = *p;
    }

    return r - result;
}

/*************************************************************************/
/* copy the arg text into the result-space, but substitute the variables */
/* %0..%9 with the real variables                                        */
/*************************************************************************/
static void substitute_ivars(const char *arg, char *result)
{
    int nest = 0;
    int numands, n;
    const char *ARG=arg;
    int valuelen, len=strlen(arg);

    if (!pvars)
    {
        strcpy(result, arg);
        return;
    }
    while (*arg)
    {
        if (*arg == '%')
        {               /* substitute variable */
            numands = 1;        /* at least one */
            while (*(arg + numands) == '%')
                numands++;
            if (isadigit(*(arg + numands)) && numands == (nest + 1))
            {
                n = *(arg + numands) - '0';
                valuelen=strlen((*pvars)[n]);
                if ((len+=valuelen-numands-1) > BUFFER_SIZE-10)
                {
                    len-=valuelen-numands-1;
                    if (!aborting)
                    {
                        tintin_eprintf(0, "#ERROR: command+vars too long in {%s}.", ARG);
                        aborting=true;
                    }
                    goto novar1;
                }
                strcpy(result, (*pvars)[n]);
                arg = arg + numands + 1;
                result += valuelen;
            }
            else
            {
novar1:
                memcpy(result, arg, numands + 1);
                arg += numands + 1;
                result += numands + 1;
            }
            in_alias=false; /* not a simple alias */
        }
        else if (*arg == '$')
        {               /* substitute variable */
            numands = 1;        /* at least one */
            while (*(arg + numands) == '$')
                numands++;
            if (isadigit(*(arg + numands)) && numands == (nest + 1))
            {
                n = *(arg + numands) - '0';
                valuelen=strlen((*pvars)[n]);
                if ((len+=valuelen-numands-1) > BUFFER_SIZE-10)
                {
                    len-=valuelen-numands-1;
                    if (!aborting)
                    {
                        tintin_eprintf(0, "#ERROR: command+vars too long in {%s}.", ARG);
                        aborting=true;
                    }
                    goto novar2;
                }
                result += defang_var(result, (*pvars)[n]);
                arg = arg + numands + 1;
            }
            else
            {
novar2:
                memcpy(result, arg, numands);
                arg += numands;
                result += numands;
            }
            in_alias=false; /* not a simple alias */
        }
        else if (*arg == BRACE_OPEN)
        {
            nest++;
            *result++ = *arg++;
        }
        else if (*arg == BRACE_CLOSE)
        {
            nest--;
            *result++ = *arg++;
        }
        else if (*arg == '\\' && nest == 0)
        {
            while (*arg == '\\')
                *result++ = *arg++;
            if (*arg == '%')
            {
                result--;
                *result++ = *arg++;
                if (*arg)
                    *result++ = *arg++;
            }
        }
        else
            *result++ = *arg++;
    }
    *result = '\0';
}


void set_variable(const char *left, const char *right, session *ses)
{
    set_hash(ses->myvars, left, right);
    varnum++;       /* we don't care for exactness of this */
    tintin_printf(MSG_VARIABLE, ses, "#Ok. $%s is now set to {%s}.", left, right);
}

static int builtin_var(const char *varname, char *value, session *ses)
{
    if (!strcmp(varname, "secstotick"))
        sprintf(value, "%lld", timetilltick(ses)/NANO);
    else if (!strcmp(varname, "TIMETOTICK"))
    {
        timens_t tt = timetilltick(ses);
        sprintf(value, "%lld.%09ld", tt/NANO, labs(tt%NANO));
    }
    else if (!strcmp(varname, "LINES"))
        sprintf(value, "%d", LINES);
    else if (!strcmp(varname, "COLS"))
        sprintf(value, "%d", COLS);
    else if (!strcmp(varname, "PATH"))
        path2var(value, ses);
    else if (!strcmp(varname, "IDLETIME"))
        nsecstr(value, current_time()-ses->idle_since);
    else if (!strcmp(varname, "SERVERIDLE"))
        nsecstr(value, current_time()-ses->server_idle_since);
    else if (!strcmp(varname, "USERIDLE"))
        nsecstr(value, current_time()-idle_since);
    else if (!strcmp(varname, "LINENUM"))
        sprintf(value, "%llu", ses->linenum);
    else if (_ && (!strcmp(varname, "LINE")
                   || !strcmp(varname, "_")))
        strcpy(value, _);
    else if (!strcmp(varname, "SESSION"))
        strcpy(value, ses->name);
    else if (!strcmp(varname, "SESSIONS"))
        seslist(value);
    else if (!strcmp(varname, "ASESSION"))
        strcpy(value, activesession->name);
    else if (!strcmp(varname, "LOGFILE"))
        strcpy(value, ses->logfile?ses->logname:"");
    else if (!strcmp(varname, "RANDOM") || !strcmp(varname, "_random"))
        sprintf(value, "%d", rand());
    else if (!strcmp(varname, "_time"))
    {
        timens_t age = current_time() - start_time;
        sprintf(value, "%lld", age/NANO);
    }
    else if (!strcmp(varname, "STARTTIME"))
        sprintf(value, "%lld.%09lld", start_time/NANO, start_time%NANO);
    else if (!strcmp(varname, "TIME"))
    {
        timens_t ct = current_time();
        sprintf(value, "%lld.%09lld", ct/NANO, ct%NANO);
    }
    else if (!strcmp(varname, "_clock"))
        sprintf(value, "%ld", (long int)time(0));
    else if (!strcmp(varname, "_msec"))
    {
        timens_t age = current_time() - start_time;
        sprintf(value, "%lld", age / (NANO/1000));
    }
    else if (!strcmp(varname, "HOME"))
    {
        const char *v=getenv("HOME");
        if (v)
            snprintf(value, BUFFER_SIZE, "%s", v);
        else
            *value=0;
    }
    else
        return 0;
    return 1;
}

void substitute_myvars(const char *arg, char *result, session *ses, int recur)
{
    char varname[BUFFER_SIZE], value[BUFFER_SIZE];
    int nest = 0;
    int len=strlen(arg);

    while (*arg)
    {
        if (*arg == '$')
        {                           /* substitute variable */
            bool specvar;
            int counter = 0, varlen = 0;
            while (*(arg + counter) == '$')
                counter++;

            /* ${name} code added by Sverre Normann */
            if (*(arg+counter) != BRACE_OPEN)
            {
                /* ordinary variable whose name contains no spaces and special characters  */
                specvar = false;
                while (is7alpha(*(arg+varlen+counter))||
                       (*(arg+varlen+counter)=='_')||
                       isadigit(*(arg+varlen+counter)))
                    varlen++;
                if (varlen>0)
                    memcpy(varname, arg+counter, varlen);
                *(varname + varlen) = '\0';
            }
            else
            {
                /* variable with name containing non-alpha characters e.g ${a b} */
                specvar = true;
                get_arg_in_braces(arg + counter, varname, 0);
                varlen=strlen(varname);
                if (recur < 8)
                {
                    substitute_ivars(varname, value);
                    /* RECURSIVE CALL */
                    substitute_myvars(value, varname, ses, recur + 1);
                }
            }

            if (specvar) varlen += 2; /* 2*DELIMITERS e.g. {} */

            char *v = 0;

            if (counter == nest + 1)
            {
                v = get_hash(ses->myvars, varname);
                if (!v && builtin_var(varname, value, ses))
                    v = value;
            }

            if (v)
            {
                int valuelen = strlen(v);
                if ((len+=valuelen-counter-varlen) > BUFFER_SIZE-10)
                {
                    if (!aborting)
                    {
                        tintin_eprintf(ses, "#ERROR: command+variables too long while substituting $%s%s%s.",
                            specvar?"{":"", varname, specvar?"}":"");
                        aborting=true;
                    }
                    len-=valuelen-counter-varlen;
                    v = 0;
                }
                else
                {
                    strcpy(result, v);
                    result += valuelen;
                    arg += counter + varlen;
                }
            }

            if (!v)
            {
                memcpy(result, arg, counter + varlen);
                result += varlen + counter;
                arg += varlen + counter;
            }
        }
        else if (*arg == BRACE_OPEN)
        {
            nest++;
            *result++ = *arg++;
        }
        else if (*arg == BRACE_CLOSE)
        {
            nest--;
            *result++ = *arg++;
        }
        else if (*arg == '\\' && *(arg + 1) == '$' && nest == 0)
        {
            arg++;
            *result++ = *arg++;
            len--;
        }
        else
            *result++ = *arg++;
    }

    *result = '\0';
}


void substitute_vars(const char *string, char *result, session *ses)
{
    char arg[BUFFER_SIZE];

    substitute_ivars(string, arg);
    substitute_myvars(arg, result, ses, 0);
}
