#include "tintin.h"
#include "kbtree.h"
#include "protos/glob.h"
#include "protos/llist.h"
#include "protos/print.h"
#include "protos/utils.h"

static int tripcmp(const trip_t a, const trip_t b)
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

/**/ KBTREE_CODE(trip, trip_t, tripcmp)

kbtree_t(trip)* init_tlist(void)
{
    return kb_init(trip, KB_DEFAULT_SIZE);
}

void kill_tlist(kbtree_t(trip) *l)
{
    kbitr_t itr;

    for (kb_itr_first(trip, l, &itr); kb_itr_valid(&itr); kb_itr_next(trip, l, &itr))
    {
        trip_t i = kb_itr_key(trip_t, &itr);
        free(i->left);
        free(i->right);
        free(i->pr);
        free(i);
    }

    kb_destroy(trip, l);
}

void show_trip(const trip_t t)
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
        const trip_t t = *kb_get(trip, l, &srch);
        if (!t)
            return false;
        if (msg)
            tintin_printf(0, msg);
        show_trip(t);
        return true;
    }

    bool had_any = false;
    kbitr_t itr;

    for (kb_itr_first(trip, l, &itr); kb_itr_valid(&itr); kb_itr_next(trip, l, &itr))
    {
        const trip_t t = kb_itr_key(trip_t, &itr);
        if (pat && !match(t->left, pat))
            continue;
        if (!had_any)
        {
            had_any = true;
            if (msg)
                tintin_printf(0, msg);
        }
        show_trip(t);
    }

    return had_any;
}

bool delete_tlist(kbtree_t(trip) *l, const char *pat, const char *msg)
{
    if (pat && is_literal(pat))
    {
        struct trip srch = {(char*)pat, 0, 0};
        trip_t t = kb_del(trip, l, &srch);
        if (!t)
            return false;
        if (msg)
            tintin_printf(0, msg, t->left);
        return true;
    }

    bool had_any = false;
    kbitr_t itr;

    for (kb_itr_first(trip, l, &itr); kb_itr_valid(&itr); kb_itr_next(trip, l, &itr))
    {
        const trip_t t = kb_itr_key(trip_t, &itr);
        if (pat && !match(pat, t->left))
            continue;
        if (!had_any)
            had_any = true;
        if (msg)
            tintin_printf(0, msg, t->left);
        kb_del(trip, l, t);
    }

    return had_any;
}

kbtree_t(trip) *copy_tlist(kbtree_t(trip) *a)
{
    kbtree_t(trip) *b = kb_init(trip, KB_DEFAULT_SIZE);
    kbitr_t itr;

    kb_itr_first(trip, a, &itr);
    for (kb_itr_first(trip, a, &itr); kb_itr_valid(&itr); kb_itr_next(trip, a, &itr))
    {
        const trip_t old = kb_itr_key(trip_t, &itr);
        trip_t new = MALLOC(sizeof(struct trip));
        new->left = mystrdup(old->left);
        new->right = mystrdup(old->right);
        new->pr = mystrdup(old->pr);
        kb_put(trip, b, new);
    }

    return b;
}

int count_tlist(kbtree_t(trip) *s)
{
    return kb_size(s);
}
