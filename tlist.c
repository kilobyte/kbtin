#include "tintin.h"
#include "kbtree.h"
#include "protos/glob.h"
#include "protos/llist.h"
#include "protos/print.h"
#include "protos/utils.h"

static int tripcmp(const ptrip a, const ptrip b)
{
    if (a->pr && b->pr)
    {
        // we should assert that either or none have a priority,
        // but pattern searches don't supply a priority.
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
        free(i);
    ENDITER

    kb_destroy(trip, l);
}

void show_trip(const ptrip t)
{
    if (t->pr)
        tintin_printf(0, "~7~{%s~7~}={%s~7~} @ {%s}", t->left, t->right, t->pr);
    else
        tintin_printf(0, "~7~{%s~7~}={%s~7~}", t->left, t->right);
}

bool show_tlist(kbtree_t(trip) *l, const char *pat, const char *msg)
{
    if (pat && is_literal(pat))
    {
        struct trip srch = {(char*)pat, 0, 0};
        const ptrip t = *kb_get(trip, l, &srch);
        if (!t)
            return false;
        if (msg)
            tintin_printf(0, msg);
        show_trip(t);
        return true;
    }

    bool had_any = false;

    TRIP_ITER(l, t)
        if (pat && !match(t->left, pat))
            continue;
        if (!had_any)
        {
            had_any = true;
            if (msg)
                tintin_printf(0, msg);
        }
        show_trip(t);
    ENDITER

    return had_any;
}

bool delete_tlist(kbtree_t(trip) *l, const char *pat, const char *msg)
{
    if (pat && is_literal(pat))
    {
        struct trip srch = {(char*)pat, 0, 0};
        ptrip t = kb_del(trip, l, &srch);
        if (!t)
            return false;
        if (msg)
            tintin_printf(0, msg, t->left);
        return true;
    }

    bool had_any = false;

    TRIP_ITER(l, t)
        if (pat && !match(pat, t->left))
            continue;
        if (!had_any)
            had_any = true;
        if (msg)
            tintin_printf(0, msg, t->left);
        kb_del(trip, l, t);
    ENDITER

    return had_any;
}

kbtree_t(trip) *copy_tlist(kbtree_t(trip) *a)
{
    kbtree_t(trip) *b = kb_init(trip, KB_DEFAULT_SIZE);

    TRIP_ITER(a, old)
        ptrip new = MALLOC(sizeof(struct trip));
        new->left = mystrdup(old->left);
        new->right = mystrdup(old->right);
        new->pr = mystrdup(old->pr);
        kb_put(trip, b, new);
    ENDITER

    return b;
}

int count_tlist(kbtree_t(trip) *s)
{
    return kb_size(s);
}
