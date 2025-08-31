#include "tintin.h"
#include "protos/action.h"
#include "protos/alias.h"
#include "protos/globals.h"
#include "protos/hash.h"
#include "protos/misc.h"
#include "protos/print.h"
#include "protos/parse.h"
#include "protos/user.h"
#include "protos/string.h"

static struct hashtable *keynames;

static const char *KEYNAMES[]=
{
    "ESC[[A",       "F1",
    "ESC[[B",       "F2",
    "ESC[[C",       "F3",
    "ESC[[D",       "F4",
    "ESC[[E",       "F5",
    "ESC[11~",      "F1",
    "ESC[12~",      "F2",
    "ESC[13~",      "F3",
    "ESC[14~",      "F4",
    "ESC[15~",      "F5",
    "ESC[17~",      "F6",
    "ESC[18~",      "F7",
    "ESC[19~",      "F8",
    "ESC[20~",      "F9",
    "ESC[21~",      "F10",
    "ESC[23~",      "F11",
    "ESC[24~",      "F12",
    "ESC[25~",      "F13",
    "ESC[26~",      "F14",
    "ESC[27~",      "F15",
    "ESC[28~",      "F16",
    "ESC[29~",      "F17",
    "ESC[30~",      "F18",
    "ESC[31~",      "F19",
    "ESC[32~",      "F20",
    "ESC[33~",      "F21",
    "ESC[34~",      "F22",
    "ESC[35~",      "F23",
    "ESC[36~",      "F24",
    "ESC[A",        "UpArrow",
    "ESC[B",        "DownArrow",
    "ESC[C",        "RightArrow",
    "ESC[D",        "LeftArrow",
    "ESC[E",        "MidArrow",
    "ESC[F",        "PgDn",
    "ESC[G",        "MidArrow",
    "ESC[H",        "Home",
    "ESC[P",        "Pause",
    "ESC[1~",       "Home",
    "ESC[2~",       "Ins",
    "ESC[3~",       "Del",
    "ESC[4~",       "End",
    "ESC[5~",       "PgUp",
    "ESC[6~",       "PgDn",
    "ESC[7~",       "Home",
    "ESC[8~",       "End",
    "ESC[OA",       "UpArrow",      /* alternate cursor mode */
    "ESC[OB",       "DownArrow",
    "ESC[OC",       "RightArrow",
    "ESC[OD",       "LeftArrow",
    "ESC[OE",       "MidArrow",
    "ESCOE",        "Kpad5",        /* screen on vte */
    "ESCOM",        "KpadEnter",    /* alternate keypad mode */
    "ESCOP",        "KpadNumLock",
    "ESCOQ",        "KpadDivide",
    "ESCOR",        "KpadMultiply",
    "ESCOS",        "KpadMinus",
    "ESCOX",        "KpadEqual",
    "ESCOj",        "KpadMultiply",
    "ESCOk",        "KpadPlus",
    "ESCOl",        "KpadPlus",
    "ESCOm",        "KpadMinus",
    "ESCOn",        "KpadDot",
    "ESCOo",        "KpadMinus",
    "ESCOp",        "Kpad0",
    "ESCOq",        "Kpad1",
    "ESCOr",        "Kpad2",
    "ESCOs",        "Kpad3",
    "ESCOt",        "Kpad4",
    "ESCOu",        "Kpad5",
    "ESCOv",        "Kpad6",
    "ESCOw",        "Kpad7",
    "ESCOx",        "Kpad8",
    "ESCOy",        "Kpad9",
    "",             "",
};

static const char *NORMAL_KEYNAMES[]=
{
    "ESCOP",        "KpadNumLock",
    "ESCOQ",        "KpadDivide",
    "ESCOR",        "KpadMultiply",
    "ESCOS",        "KpadMinus",
    "ESCOo",        "KpadDivide",
    "",             "",
};

static const char *XTERM_KEYNAMES[]=
{
    "ESCOP",        "F1",
    "ESCOQ",        "F2",
    "ESCOR",        "F3",
    "ESCOS",        "F4",
    "ESCOm",        "KpadMinus",
    "",             "",
};

/*********************/
/* the #bind command */
/*********************/
void bind_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE], right[BUFFER_SIZE];

    if (!ui_keyboard)
    {
        tintin_eprintf(ses, "#UI: no access to keyboard => no keybindings");
        return;
    }
    arg = get_arg_in_braces(arg, left, 0);
    arg = get_arg_in_braces(arg, right, 1);

    if (*left && *right)
    {
        set_hash(ses->binds, left, right);
        if (ses->mesvar[MSG_BIND])
            tintin_printf(ses, "#Ok. {%s} is now bound to {%s}.", left, right);
        bindnum++;
        return;
    }
    show_hashlist(ses, ses->binds, left,
        "#Bound keys:",
        "#No match(es) found for {%s}");
}

/***********************/
/* the #unbind command */
/***********************/
void unbind_command(const char *arg, session *ses)
{
    char left[BUFFER_SIZE];

    if (!ui_keyboard)
    {
        tintin_eprintf(ses, "#UI: no access to keyboard => no keybindings");
        return;
    }
    arg = get_arg(arg, left, 1, ses);
    delete_hashlist(ses, ses->binds, left,
        ses->mesvar[MSG_BIND]? "#Ok. {%s} is no longer bound." : 0,
        ses->mesvar[MSG_BIND]? "#No match(es) found for {%s}" : 0);
}


static const char *bitted(const char *key, uint8_t bits)
{
    if (!bits)
        return key;

    static char esckey[64];
    snprintf(esckey, sizeof(esckey), "%s%s%s%s",
        bits&4? "Ctrl-" : "",
        bits&2? "Alt-" : "",
        bits&1? "Shift-" : "",
        key);
    return esckey;
}


bool find_bind(const char *key, uint8_t bits, int msg, session *ses)
{
    char *val;

#ifdef KEYBOARD_DEBUG
    tintin_printf(ses, "~3~[~11~%s~3~]~7~", bitted(key, bits));
#endif
    if ((val=get_hash(ses->binds, bitted(key, bits))))
    {          /* search twice, both for raw key code and key name */
        parse_input(val, true, ses);
        recursion=0;
        return true;
    }
    if ((val=get_hash(keynames, key)))
    {
        key=val;
#ifdef KEYBOARD_DEBUG
        tintin_printf(ses, "~3~→ [~11~%s~3~]~7~", bitted(key, bits));
#endif
        if ((val=get_hash(ses->binds, bitted(key, bits))))
        {
            parse_input(val, true, ses);
            recursion=0;
            return true;
        }
    }
    if (msg)
        tintin_printf(ses, "#Unbound keycode: %s", bitted(key, bits));
    return false;
}


void init_bind(void)
{
    keynames=init_hash();
    if (!ui_keyboard)
        return;
    for (const char**n=KEYNAMES;**n;n+=2)
        set_hash_nostring(keynames, n[0], n[1]);
}

void cleanup_bind(void)
{
    kill_hash_nostring(keynames);
}

void bind_xterm(bool xterm)
{
    if (xterm)
        for (const char**n=XTERM_KEYNAMES;**n;n+=2)
            set_hash_nostring(keynames, n[0], n[1]);
    else
        for (const char**n=NORMAL_KEYNAMES;**n;n+=2)
            set_hash_nostring(keynames, n[0], n[1]);
}
