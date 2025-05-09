#include "tintin.h"
#include "protos/globals.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/utils.h"
#include <sys/types.h>
#include <regex.h>


extern struct session *if_command(const char *arg, struct session *ses);

static bool check_regexp(char *line, char *action, pvars_t *vars, struct session *ses)
{
    regex_t preg;
    regmatch_t pmatch[10];

    if (regcomp(&preg, action, REG_EXTENDED))
    {
        tintin_eprintf(ses, "#invalid regular expression: {%s}", action);
        return false;
    }
    if (regexec(&preg, line, vars?10:0, pmatch, 0))
    {
        regfree(&preg);
        return false;
    }
    if (vars)
        for (int i = 0; i < 10; i++)
        {
            if (pmatch[i].rm_so != -1)
            {
                memcpy((*vars)[i], line+pmatch[i].rm_so,
                                   pmatch[i].rm_eo-pmatch[i].rm_so);
                *((*vars)[i] + pmatch[i].rm_eo-pmatch[i].rm_so) = '\0';
            }
            else
                (*vars)[i][0]=0;
        }
    regfree(&preg);
    return true;
}

/*********************/
/* the #grep command */
/*********************/
struct session *grep_command(const char *arg, struct session *ses)
{
    pvars_t vars, *lastpvars;
    char left[BUFFER_SIZE], line[BUFFER_SIZE], right[BUFFER_SIZE];

    arg=get_arg(arg, left, 0, ses);
    arg=get_arg(arg, line, 0, ses);
    arg=get_arg_in_braces(arg, right, 0);

    if (!*left || !*right)
    {
        tintin_eprintf(ses, "#ERROR: valid syntax is: #grep <pattern> <line> <command> [#else ...]");
        return ses;
    }

    if (check_regexp(line, left, &vars, ses))
    {
        lastpvars = pvars;
        pvars = &vars;
        ses = parse_input(right, true, ses);
        pvars = lastpvars;
        return ses;
    }

    return ifelse("grep", arg, ses);
}


/********************/
/* the #grep inline */
/********************/
int grep_inline(const char *arg, struct session *ses)
{
    char left[BUFFER_SIZE], line[BUFFER_SIZE];

    arg=get_arg(arg, left, 0, ses);
    arg=get_arg(arg, line, 1, ses);

    if (!*left)
    {
        tintin_eprintf(ses, "#ERROR: valid syntax is: (#grep <pattern> <line>)");
        return 0;
    }
    return check_regexp(line, left, 0, ses);
}
