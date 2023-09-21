#include "tintin.h"
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

typedef enum
{
    PRIO_LPAREN,
    PRIO_RPAREN,
    PRIO_NOT,
    PRIO_MULT,
    PRIO_ADD,
    PRIO_INEQUAL,
    PRIO_EQUAL,
    PRIO_NONEQUAL, // wat?
    PRIO_AND,
    PRIO_OR,
    PRIO_LITERAL,
    PRIO_NONE
} prio_t;

static struct stacks
{
    int pos;
    prio_t prio;
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
    while (*line && !isaspace(*line))
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
    else if (is_abrev(command, "sqrt"))
        *res=sqrt_inline(line, ses);
    else if (is_abrev(command, "abs"))
        *res=abs_inline(line, ses);
    else if (is_abrev(command, "round"))
        *res=round_inline(line, ses);
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
            if (stacks[i].prio == PRIO_LPAREN)
            {
                begin = i;
            }
            else if (stacks[i].prio == PRIO_RPAREN)
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

// comparing strings inside [ ] with = and !=
static char *stringcmp(char *ptr, int i, struct session *ses)
{
    bool result, should_differ;
    bool regex=false; /* false=strncmp, true=regex match */
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

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
    stacks[i].prio = PRIO_LITERAL;
    stacks[i].val = N(result == should_differ);
    return ptr;
}

static bool conv_to_nums(char *arg, struct session *ses)
{
    char temp[BUFFER_SIZE];

    int i = 0;
    char *ptr = arg;
    while (*ptr)
    {
        if (*ptr == tintin_char)
            /* inline commands */
        {
            ptr=(char*)get_inline(ptr+1, temp)-1;
            num_t res;
            if (!do_inline(temp, &res, ses))
                return false;
            stacks[i].val=res;
            stacks[i].prio=PRIO_LITERAL;
        }
        else switch (*ptr)
        {
        case ' ': case '\t': case '\n': case 12: case '\v':
            break;

        case '[':
            ptr = stringcmp(ptr, i, ses);
            if (!ptr)
                return false;
            break;

        case '$':
            if (ses->mesvar[MSG_VARIABLE])
                tintin_eprintf(ses, "#Undefined variable in {%s}.", arg);
            stacks[i].prio = PRIO_LITERAL;
            stacks[i].val = N(0);
            if (*(++ptr)==BRACE_OPEN)
                ptr=(char*)get_arg_in_braces(ptr, temp, 0);
            else
                while (is7alpha(*ptr) || *ptr=='_' || isadigit(*ptr))
                    ptr++;
            ptr--;
            break;

        case '(':
            stacks[i].prio = PRIO_LPAREN;
            break;
        case ')':
            stacks[i].prio = PRIO_RPAREN;
            break;
        case '!':
            if (*(ptr + 1) == '=')
                stacks[i].prio = PRIO_NONEQUAL,
                ptr++;
            else
                stacks[i].prio = PRIO_NOT;
            break;
        case '*':
            stacks[i].prio = PRIO_MULT;
            stacks[i].op = 0;
            break;
        case '/':
            stacks[i].prio = PRIO_MULT;
            if (ptr[1] == '/') /* / is integer division, // fractional */
                stacks[i].op = 2, ptr++;
            else
                stacks[i].op = 1;
            break;
        case '%':
            stacks[i].prio = PRIO_MULT;
            stacks[i].op = 3;
            break;
        case '+':
            stacks[i].prio = PRIO_ADD;
            stacks[i].op = 2;
            break;
        case '-':
            if (i > 0 && stacks[i - 1].prio == PRIO_LITERAL)
            {
                stacks[i].prio = PRIO_ADD;
                stacks[i].op = 3;
            }
            else
            {
                stacks[i].val = str2num(ptr, &ptr);
                stacks[i].prio = PRIO_LITERAL;
                ptr--;
            }
            break;
        case '>':
            if (*(ptr + 1) == '=')
            {
                stacks[i].prio = PRIO_INEQUAL;
                stacks[i].op = 4;
                ptr++;
            }
            else
            {
                stacks[i].prio = PRIO_INEQUAL;
                stacks[i].op = 5;
            }
            break;
        case '<':
            if (*(ptr + 1) == '=')
            {
                ptr++;
                stacks[i].prio = PRIO_INEQUAL;
                stacks[i].op = 6;
            }
            else
            {
                stacks[i].prio = PRIO_INEQUAL;
                stacks[i].op = 7;
            }
            break;
        case '=':
            stacks[i].prio = PRIO_EQUAL;
            if (*(ptr + 1) == '=')
                ptr++;
            break;
        case '&':
            stacks[i].prio = PRIO_AND;
            if (*(ptr + 1) == '&')
                ptr++;
            break;
        case '|':
            stacks[i].prio = PRIO_OR;
            if (*(ptr + 1) == '|')
                ptr++;
            break;
        case '0' ... '9':
            stacks[i].prio = PRIO_LITERAL;
            stacks[i].val = str2num(ptr, &ptr);
            ptr--;
            break;
        case 'T':
            stacks[i].prio = PRIO_LITERAL;
            stacks[i].val = N(1);
            break;
        case 'F':
            stacks[i].prio = PRIO_LITERAL;
            stacks[i].val = N(0);
            break;
        default:
            tintin_eprintf(ses, "#Error. Invalid expression in #if or #math in {%s}.", arg);
            return false;
        }

        if (!isaspace(*ptr))
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
        prio_t highest = PRIO_NONE;
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

        if (highest == PRIO_LITERAL)
        {
            if (begin > -1)
            {
                stacks[begin].prio = PRIO_LITERAL;
                stacks[begin].val = stacks[loc].val;
                stacks[begin].pos = stacks[end].pos;
                return true;
            }
            else
            {
                stacks[0].pos = stacks[end].pos;
                stacks[0].prio = PRIO_LITERAL;
                stacks[0].val = stacks[loc].val;
                return true;
            }
        }
        else if (highest == PRIO_NOT)
        {
            int next = stacks[loc].pos;
            if (stacks[next].prio != PRIO_LITERAL || stacks[next].pos == 0)
                return false;
            stacks[loc].pos = stacks[next].pos;
            stacks[loc].prio = PRIO_LITERAL;
            stacks[loc].val = N(!stacks[next].val);
        }
        else
        {
            if (loc < 0)
                return false;

            int next = stacks[loc].pos;
            if (ploc == -1 || stacks[next].pos == 0 || stacks[next].prio != PRIO_LITERAL)
                return false;
            if (stacks[ploc].prio != PRIO_LITERAL)
                return false;
            switch (highest)
            {
            case PRIO_MULT:     /* *,/ */
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
            case PRIO_ADD:      /* +,- */
                stacks[ploc].pos = stacks[next].pos;
                if (stacks[loc].op==2)
                    stacks[ploc].val += stacks[next].val;
                else
                    stacks[ploc].val -= stacks[next].val;
                break;
            case PRIO_INEQUAL:  /* >,>=,<,<= */
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
            case PRIO_EQUAL:    /* == */
                stacks[ploc].pos = stacks[next].pos;
                stacks[ploc].val = N(stacks[ploc].val == stacks[next].val);
                break;
            case PRIO_NONEQUAL: /* != */
                stacks[ploc].pos = stacks[next].pos;
                stacks[ploc].val = N(stacks[ploc].val != stacks[next].val);
                break;
            case PRIO_AND:      /* && */
                stacks[ploc].pos = stacks[next].pos;
                stacks[ploc].val = N(stacks[ploc].val && stacks[next].val);
                break;
            case PRIO_OR:       /* || */
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
