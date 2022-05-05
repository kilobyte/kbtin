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
    STR_ITER(l, p)
        free(p);
    ENDITER

    kb_destroy(str, l);
}

void show_slist(kbtree_t(str) *l)
{
    STR_ITER(l, p)
        tintin_printf(0, "~7~{%s~7~}", p);
    ENDITER
}

kbtree_t(str) *copy_slist(kbtree_t(str) *a)
{
    kbtree_t(str) *b = kb_init(str, KB_DEFAULT_SIZE);

    STR_ITER(a, p)
        kb_put(str, b, mystrdup(p));
    ENDITER

    return b;
}

int count_slist(kbtree_t(str) *s)
{
    return kb_size(s);
}
