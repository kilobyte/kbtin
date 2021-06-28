#include "tintin.h"
#include "kbtree.h"
#include "protos/print.h"

/**/ KBTREE_CODE(str, char*, strcmp)
typedef kbtree_t(str) slist;

struct slist* init_slist(void)
{
    return (struct slist*)kb_init(str, KB_DEFAULT_SIZE);
}

void kill_slist(struct slist* s)
{
    slist *l = (slist*)s;
    kbitr_t itr;

    kb_itr_first(str, l, &itr);
    for (; kb_itr_valid(&itr); kb_itr_next(str, l, &itr))
        free(kb_itr_key(char*, &itr));

    kb_destroy(str, (slist*)s);
}

void show_slist(struct slist* s)
{
    slist *l = (slist*)s;
    kbitr_t itr;

    kb_itr_first(str, l, &itr);
    for (; kb_itr_valid(&itr); kb_itr_next(str, l, &itr))
    {
        const char *p = kb_itr_key(char*, &itr);
        tintin_printf(0, "~7~{%s~7~}", p);
    }
}

struct slist* copy_slist(struct slist *src)
{
    slist *a = (slist*)src;
    slist *b = kb_init(str, KB_DEFAULT_SIZE);
    kbitr_t itr;

    kb_itr_first(str, a, &itr);
    for (; kb_itr_valid(&itr); kb_itr_next(str, a, &itr))
    {
        const char *p = kb_itr_key(char*, &itr);
        kb_put(str, b, p);
    }

    return (struct slist*)b;
}

int count_slist(struct slist *s)
{
    return kb_size((slist*)s);
}
