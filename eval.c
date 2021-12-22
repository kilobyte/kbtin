#include "tintin.h"
#include "assert.h"
#include "protos/action.h"
#include "protos/glob.h"
#include "protos/globals.h"
#include "protos/eval.h"
#include "protos/lists.h"
#include "protos/math.h"
#include "protos/print.h"
#include "protos/misc.h"
#include "protos/parse.h"
#include "protos/regexp.h"
#include "protos/utils.h"
#include "protos/string.h"
#include "protos/vars.h"

static struct stacks
{
    int pos;
    int prio;
    num_t val;
    int op;
} stacks[100];
static bool conv_to_nums(char *arg, struct session *ses);
static bool do_one_inside(int begin, int end);


/*********************/
/* the #math command */
/*********************/
void math_command(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE], temp[BUFFER_SIZE];

    line = get_arg(line, left, 0, ses);
    line = get_arg(line, right, 1, ses);
    if (!*left||!*right)
    {
        tintin_eprintf(ses, "#Syntax: #math <variable> <expression>");
        return;
    }
    num2str(temp, eval_expression(right, ses));
    set_variable(left, temp, ses);
}

/*******************/
/* the #if command */
/*******************/
struct session *if_command(const char *line, struct session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    line = get_arg(line, left, 0, ses);
    line = get_arg_in_braces(line, right, 1);

    if (!*left || !*right)
    {
        tintin_eprintf(ses, "#ERROR: valid syntax is: if <condition> <command> [#elif <condition> <command>] [...] [#else <command>]");
        return ses;
    }

    if (eval_expression(left, ses))
        ses=parse_input(right, true, ses);
    else
    {
        line = get_arg_in_braces(line, left, 0);
        if (*left == tintin_char)
        {

            if (is_abrev(left + 1, "else"))
            {
                line = get_arg_in_braces(line, right, 1);
                ses=parse_input(right, true, ses);
            }
            if (is_abrev(left + 1, "elif"))
                ses=if_command(line, ses);
        }
    }
    return ses;
}


static bool do_inline(const char *line, num_t *res, struct session *ses)
{
    char command[BUFFER_SIZE], *ptr;
    struct stacks savestacks[ARRAYSZ(stacks)];

    _Static_assert(sizeof(savestacks) == sizeof(stacks), "stacks size mismatch");
    memcpy(savestacks, stacks, sizeof(savestacks));

    ptr=command;
    while (*line&&(*line!=' '))
        *ptr++=*line++;
    *ptr=0;
    line=space_out(line);
    if (is_abrev(command, "finditem"))
        *res=N(finditem_inline(line, ses));
    else if (is_abrev(command, "isatom"))
        *res=N(isatom_inline(line, ses));
    else if (is_abrev(command, "listlength"))
        *res=N(listlength_inline(line, ses));
    else if (is_abrev(command, "strlen"))
        *res=N(strlen_inline(line, ses));
    else if (is_abrev(command, "random"))
        *res=N(random_inline(line, ses));
    else if (is_abrev(command, "grep"))
        *res=N(grep_inline(line, ses));
    else if (is_abrev(command, "strcmp"))
        *res=N(strcmp_inline(line, ses));
    else if (is_abrev(command, "match"))
        *res=N(match_inline(line, ses));
    else if (is_abrev(command, "ord"))
        *res=N(ord_inline(line, ses));
    else if (is_abrev(command, "angle"))
        *res=angle_inline(line, ses);
    else if (is_abrev(command, "sinus"))
        *res=sinus_inline(line, ses);
    else if (is_abrev(command, "cosinus"))
        *res=cosinus_inline(line, ses);
    else
    {
        tintin_eprintf(ses, "#Unknown inline command [%c%s]!", tintin_char, command);
        return false;
    }

    memcpy(stacks, savestacks, sizeof(savestacks));
    return true;
}


num_t eval_expression(char *arg, struct session *ses)
{
    if (!conv_to_nums(arg, ses))
        return 0;

    while (1)
    {
        int i = 0;
        bool flag = true;
        int begin = -1;
        int end = -1;
        int prev = -1;
        while (stacks[i].pos && flag)
        {
            if (stacks[i].prio == 0)
            {
                begin = i;
            }
            else if (stacks[i].prio == 1)
            {
                end = i;
                flag = false;
            }
            prev = i;
            i = stacks[i].pos;
        }
        if ((flag && (begin != -1)) || (!flag && (begin == -1)))
        {
            tintin_eprintf(ses, "#Unmatched parentheses error in {%s}.", arg);
            return 0;
        }
        if (flag)
        {
            if (prev == -1)
                return stacks[0].val;
            begin = -1;
            end = i;
        }
        if (!do_one_inside(begin, end))
        {
            tintin_eprintf(ses, "#Invalid expression to evaluate in {%s}", arg);
            return 0;
        }
    }
}

static bool conv_to_nums(char *arg, struct session *ses)
{
    int i, flag;
    bool result, should_differ;
    bool regex=false; /* false=strncmp, true=regex match */
    char *ptr;
    char temp[BUFFER_SIZE];
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    i = 0;
    ptr = arg;
    while (*ptr)
    {
        if (*ptr == ' ') ;
        else if (*ptr == tintin_char)
            /* inline commands */
        {
            ptr=(char*)get_inline(ptr+1, temp)-1;
            num_t res;
            if (!do_inline(temp, &res, ses))
                return false;
            stacks[i].val=res;
            stacks[i].prio=15;
        }
        /* jku: comparing strings with = and != */
        else if (*ptr == '[')
        {
            ptr++;
            char *tptr=left;
            while ((*ptr) && (*ptr != ']') && (*ptr != '=') && (*ptr != '!'))
            {
                *tptr = *ptr;
                ptr++;
                tptr++;
            }
            *tptr='\0';
            if (!*ptr)
                return false; /* error */
            if (*ptr == ']')
                tintin_eprintf(ses, "#Compare %s to what ? (only one var between [ ])", left);
            switch (*ptr)
            {
            case '!' :
                ptr++;
                should_differ=true;
                switch (*ptr)
                {
                case '=' : regex=false; ptr++; break;
                case '~' : regex=true; ptr++; break;
                default : return false;
                }
                break;
            case '=' :
                ptr++;
                should_differ=false;
                switch (*ptr)
                {
                case '=' : regex=false; ptr++; break;
                case '~' : regex=true; ptr++; break;
                default : break;
                }
                break;
            default : return false;
            }

            tptr=right;
            while ((*ptr) && (*ptr != ']'))
            {
                *tptr = *ptr;
                ptr++;
                tptr++;
            }
            *tptr='\0';
            if (!*ptr)
                return false;
            if (regex)
                result = !match(right, left);
            else
                result = strcmp(left, right);
            if (result == should_differ)
            { /* success */
                stacks[i].prio = 15;
                stacks[i].val = N(1);
            }
            else
            {
                stacks[i].prio = 15;
                stacks[i].val = N(0);
            }
        }
        /* jku: end of comparing strings */
        /* jku: undefined variables are now assigned value 0 (false) */
        else if (*ptr == '$')
        {
            if (ses->mesvar[MSG_VARIABLE])
                tintin_eprintf(ses, "#Undefined variable in {%s}.", arg);
            stacks[i].prio = 15;
            stacks[i].val = N(0);
            if (*(++ptr)==BRACE_OPEN)
            {
                ptr=(char*)get_arg_in_braces(ptr, temp, 0);
            }
            else
            {
                while (isalpha(*ptr) || *ptr=='_' || isadigit(*ptr))
                    ptr++;
            }
            ptr--;
        }
        /* jku: end of changes */
        else if (*ptr == '(')
            stacks[i].prio = 0;
        else if (*ptr == ')')
            stacks[i].prio = 1;
        else if (*ptr == '!')
            if (*(ptr + 1) == '=')
                stacks[i].prio = 12,
                ptr++;
            else
                stacks[i].prio = 2;
        else if (*ptr == '*')
        {
            stacks[i].prio = 3;
            stacks[i].op = 0;
        }
        else if (*ptr == '/')
        {
            stacks[i].prio = 3;
            if (ptr[1] == '/') /* / is integer division, // fractional */
                stacks[i].op = 2, ptr++;
            else
                stacks[i].op = 1;
        }
        else if (*ptr == '%')
        {
            stacks[i].prio = 3;
            stacks[i].op = 3;
        }
        else if (*ptr == '+')
        {
            stacks[i].prio = 5;
            stacks[i].op = 2;
        }
        else if (*ptr == '-')
        {
            flag = -1;
            if (i > 0)
                flag = stacks[i - 1].prio;
            if (flag == 15)
            {
                stacks[i].prio = 5;
                stacks[i].op = 3;
            }
            else
            {
                stacks[i].val = str2num(ptr, &ptr);
                stacks[i].prio = 15;
                ptr--;
            }
        }
        else if (*ptr == '>')
        {
            if (*(ptr + 1) == '=')
            {
                stacks[i].prio = 8;
                stacks[i].op = 4;
                ptr++;
            }
            else
            {
                stacks[i].prio = 8;
                stacks[i].op = 5;
            }
        }
        else if (*ptr == '<')
        {
            if (*(ptr + 1) == '=')
            {
                ptr++;
                stacks[i].prio = 8;
                stacks[i].op = 6;
            }
            else
            {
                stacks[i].prio = 8;
                stacks[i].op = 7;
            }
        }
        else if (*ptr == '=')
        {
            stacks[i].prio = 11;
            if (*(ptr + 1) == '=')
                ptr++;
        }
        else if (*ptr == '&')
        {
            stacks[i].prio = 13;
            if (*(ptr + 1) == '&')
                ptr++;
        }
        else if (*ptr == '|')
        {
            stacks[i].prio = 14;
            if (*(ptr + 1) == '|')
                ptr++;
        }
        else if (isadigit(*ptr))
        {
            stacks[i].prio = 15;
            stacks[i].val = str2num(ptr, &ptr);
            ptr--;
        }
        else if (*ptr == 'T')
        {
            stacks[i].prio = 15;
            stacks[i].val = N(1);
        }
        else if (*ptr == 'F')
        {
            stacks[i].prio = 15;
            stacks[i].val = N(0);
        }
        else
        {
            tintin_eprintf(ses, "#Error. Invalid expression in #if or #math in {%s}.", arg);
            return false;
        }
        if (*ptr != ' ')
        {
            stacks[i].pos = i + 1;
            i++;
            if (i >= (int)ARRAYSZ(stacks))
            {
                tintin_eprintf(ses, "Error. Expression too long: {%s}", arg);
                return false;
            }
        }
        ptr++;
    }
    if (i > 0)
        stacks[i].pos = 0;
    return true;
}

static bool do_one_inside(int begin, int end)
{
    while (1)
    {
        int ptr = (begin > -1) ? stacks[begin].pos : 0;
        int highest = 16;
        int loc = -1;
        int ploc = -1;
        int prev = -1;
        while (ptr < end)
        {
            if (stacks[ptr].prio < highest)
            {
                highest = stacks[ptr].prio;
                loc = ptr;
                ploc = prev;
            }
            prev = ptr;
            ptr = stacks[ptr].pos;
        }

        if (highest == 15)
        {
            if (begin > -1)
            {
                stacks[begin].prio = 15;
                stacks[begin].val = stacks[loc].val;
                stacks[begin].pos = stacks[end].pos;
                return true;
            }
            else
            {
                stacks[0].pos = stacks[end].pos;
                stacks[0].prio = 15;
                stacks[0].val = stacks[loc].val;
                return true;
            }
        }
        else if (highest == 2)
        {
            int next = stacks[loc].pos;
            if (stacks[next].prio != 15 || stacks[next].pos == 0)
                return false;
            stacks[loc].pos = stacks[next].pos;
            stacks[loc].prio = 15;
            stacks[loc].val = N(!stacks[next].val);
        }
        else
        {
            int next;
            assert(loc >= 0);
            next = stacks[loc].pos;
            if (ploc == -1 || stacks[next].pos == 0 || stacks[next].prio != 15)
                return false;
            if (stacks[ploc].prio != 15)
                return false;
            switch (highest)
            {
            case 3:            /* highest priority is *,/ */
                stacks[ploc].pos = stacks[next].pos;
                if (stacks[loc].op==0)
                    stacks[ploc].val = nmul(stacks[ploc].val, stacks[next].val);
                else if (!stacks[next].val)
                {
                    stacks[ploc].val=0;
                    tintin_eprintf(0, "#Error: Division by zero.");
                }
                else if (stacks[loc].op==1)
                    stacks[ploc].val = N(stacks[ploc].val / stacks[next].val);
                else if (stacks[loc].op==2)
                    stacks[ploc].val = ndiv(stacks[ploc].val, stacks[next].val);
                else
                    stacks[ploc].val %= stacks[next].val;
                break;
            case 5:            /* highest priority is +,- */
                stacks[ploc].pos = stacks[next].pos;
                if (stacks[loc].op==2)
                    stacks[ploc].val += stacks[next].val;
                else
                    stacks[ploc].val -= stacks[next].val;
                break;
            case 8:            /* highest priority is >,>=,<,<= */
                stacks[ploc].pos = stacks[next].pos;
                switch (stacks[loc].op)
                {
                case 5:
                    stacks[ploc].val = N(stacks[ploc].val > stacks[next].val);
                    break;
                case 4:
                    stacks[ploc].val = N(stacks[ploc].val >= stacks[next].val);
                    break;
                case 7:
                    stacks[ploc].val = N(stacks[ploc].val < stacks[next].val);
                    break;
                case 6:
                    stacks[ploc].val = N(stacks[ploc].val <= stacks[next].val);
                }
                break;
            case 11:            /* highest priority is == */
                stacks[ploc].pos = stacks[next].pos;
                stacks[ploc].val = N(stacks[ploc].val == stacks[next].val);
                break;
            case 12:            /* highest priority is != */
                stacks[ploc].pos = stacks[next].pos;
                stacks[ploc].val = N(stacks[ploc].val != stacks[next].val);
                break;
            case 13:            /* highest priority is && */
                stacks[ploc].pos = stacks[next].pos;
                stacks[ploc].val = N(stacks[ploc].val && stacks[next].val);
                break;
            case 14:            /* highest priority is || */
                stacks[ploc].pos = stacks[next].pos;
                stacks[ploc].val = N(stacks[ploc].val || stacks[next].val);
                break;
            default:
                tintin_eprintf(0, "#Programming error *slap Bill*");
                return false;
            }
        }
    }
}
