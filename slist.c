#include "tintin.h"
#include "kbtree.h"
#include "protos/print.h"
#include "protos/utils.h"

/**/ KBTREE_CODE(str, char*, strcmp)

kbtree_t(str)* init_slist(void)
{
    return kb_init(str, KB_DEFAULT_SIZE);
}

void kill_slist(kbtree_t(str) *l)
{
    kbitr_t itr;

    kb_itr_first(str, l, &itr);
    for (; kb_itr_valid(&itr); kb_itr_next(str, l, &itr))
        free(kb_itr_key(char*, &itr));

    kb_destroy(str, l);
}

void show_slist(kbtree_t(str) *l)
{
    kbitr_t itr;

    kb_itr_first(str, l, &itr);
    for (; kb_itr_valid(&itr); kb_itr_next(str, l, &itr))
    {
        const char *p = kb_itr_key(char*, &itr);
        tintin_printf(0, "~7~{%s~7~}", p);
    }
}

kbtree_t(str) *copy_slist(kbtree_t(str) *a)
{
    kbtree_t(str) *b = kb_init(str, KB_DEFAULT_SIZE);
    kbitr_t itr;

    kb_itr_first(str, a, &itr);
    for (; kb_itr_valid(&itr); kb_itr_next(str, a, &itr))
    {
        const char *p = kb_itr_key(char*, &itr);
        kb_put(str, b, mystrdup(p));
    }

    return b;
}

int count_slist(kbtree_t(str) *s)
{
    return kb_size(s);
}
