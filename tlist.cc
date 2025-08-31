#include "tintin.h"
#include "protos/glob.h"
#include "protos/print.h"
#include "protos/utils.h"


/******************************************************************/
/* compare priorities of a and b in a semi-lexicographical order: */
/* strings generally sort in ASCIIbetical order, however numbers  */
/* sort according to their numerical values.                      */
/******************************************************************/
int prioritycmp(const char *a, const char *b)
{
    int res;

not_numeric:
    while (*a && *a==*b && !isadigit(*a))
    {
        a++;
        b++;
    }
    if (!isadigit(*a) || !isadigit(*b))
        return (*a<*b)? -1 : (*a>*b)? 1 : 0;
    while (*a=='0')
        a++;
    while (*b=='0')
        b++;
    res=0;
    while (isadigit(*a))
    {
        if (!isadigit(*b))
            return 1;
        if (*a!=*b && !res)
            res=(*a<*b)? -1 : 1;
        a++;
        b++;
    }
    if (isadigit(*b))
        return -1;
    if (res)
        return res;
    goto not_numeric;
}

/*****************************************************/
/* strcmp() that sorts '\0' later than anything else */
/*****************************************************/
static int strlongercmp(const char *a, const char *b)
{
next:
    if (!*a)
        return *b? 1 : 0;
    if (*a==*b)
    {
        a++;
        b++;
        goto next;
    }
    if (!*b || ((unsigned char)*a) < ((unsigned char)*b))
        return -1;
    return 1;
}

static int tripcmp(const ptrip a, const ptrip b)
{
    if (a->pr)
    {
        assert(b->pr);
        int r=prioritycmp(a->pr, b->pr);
        if (r)
            return r;
    }
    return strlongercmp(a->left, b->left);
}

/**/ KBTREE_CODE(trip, ptrip, tripcmp)

kbtree_t(trip)* init_tlist(void)
{
    return kb_init(trip, KB_DEFAULT_SIZE);
}

void kill_tlist(kbtree_t(trip) *l)
{
    TRIP_ITER(l, i)
        free(i->left);
        free(i->right);
        free(i->pr);
        delete i;
    ENDITER

    kb_destroy(trip, l);
}

void show_trip(const ptrip t, session *ses)
{
    if (t->pr)
        tintin_printf(ses, "~7~{%s~7~}={%s~7~} @ {%s}", t->left, t->right, t->pr);
    else
        tintin_printf(ses, "~7~{%s~7~}={%s~7~}", t->left, t->right);
}

bool show_tlist(kbtree_t(trip) *l, const char *pat, const char *msg, bool no_pr, session *ses)
{
    if (no_pr && pat && is_literal(pat))
    {
        struct trip srch = {(char*)pat, 0, 0};
        const ptrip *t = kb_get(trip, l, &srch);
        if (!t)
            return false;
        if (msg)
            tintin_printf(ses, msg);
        show_trip(*t, ses);
        return true;
    }

    bool had_any = false;

    TRIP_ITER(l, t)
        if (pat && !match(pat, t->left))
            continue;
        if (!had_any)
        {
            had_any = true;
            if (msg)
                tintin_printf(ses, msg);
        }
        show_trip(t, ses);
    ENDITER

    return had_any;
}

bool delete_tlist(kbtree_t(trip) *l, const char *pat, const char *msg, bool (*checkright)(char **right), bool no_pr, session *ses)
{
    if (no_pr && pat && is_literal(pat))
    {
        struct trip srch = {(char*)pat, 0, 0};
        ptrip *d = kb_get(trip, l, &srch);
        if (!d)
            return false;
        ptrip t = *d;
        if (checkright && checkright(&t->right))
            return false;
        kb_del(trip, l, &srch);
        if (msg)
            tintin_printf(ses, msg, t->left);
        free(t->left);
        free(t->right);
        free(t->pr);
        delete t;
        return true;
    }

    std::vector<ptrip> todel;
    todel.reserve(kb_size(l));

    TRIP_ITER(l, t)
        if (pat && !match(pat, t->left))
            continue;
        if (checkright && checkright(&t->right))
            continue;
        todel.emplace_back(t);
    ENDITER

    for (ptrip del : todel)
    {
        if (msg)
            tintin_printf(ses, msg, del->left);
        kb_del(trip, l, del);
        free(del->left);
        free(del->right);
        free(del->pr);
        delete del;
    }

    return !todel.empty();
}

kbtree_t(trip) *copy_tlist(kbtree_t(trip) *a)
{
    kbtree_t(trip) *b = kb_init(trip, KB_DEFAULT_SIZE);

    TRIP_ITER(a, old)
        ptrip nt = new trip;
        nt->left = mystrdup(old->left);
        nt->right = mystrdup(old->right);
        nt->pr = mystrdup(old->pr);
        kb_put(trip, b, nt);
    ENDITER

    return b;
}

int count_tlist(kbtree_t(trip) *s)
{
    return kb_size(s);
}
