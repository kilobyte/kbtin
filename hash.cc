#include "tintin.h"
#include "protos/glob.h"
#include "protos/utils.h"

#define DELETED_HASHENTRY ((char*)-1)

// Jenkins hash
static uint32_t hash(const char *key)
{
    uint32_t h = 0;

    while (*key)
    {
        h += *key++;
        h += h << 10;
        h ^= h >> 6;
    }
    h += h << 3;
    h ^= h >> 11;
    h += h << 15;
    return h;
}


/**********************************/
/* initialize an empty hash table */
/**********************************/
struct hashtable* init_hash(void)
{
    struct hashtable *h=TALLOC(struct hashtable);
    h->size=8;
    h->nent=0;
    h->nval=0;
    h->tab=CALLOC(8, struct hashentry);
    return h;
}


/*********************/
/* free a hash table */
/*********************/
void kill_hash(struct hashtable* h)
{
    if (h->nval)
        for (int i=0; i<h->size; i++)
        {
            if (h->tab[i].left && (h->tab[i].left!=DELETED_HASHENTRY))
            {
                SFREE(h->tab[i].left);
                SFREE(h->tab[i].right);
            }
        }
    CFREE(h->tab, h->size, struct hashentry);
    TFREE(h, struct hashtable);
}

void kill_hash_nostring(struct hashtable* h)
{
    if (h->nval)
        for (int i=0; i<h->size; i++)
        {
            if (h->tab[i].left && (h->tab[i].left!=DELETED_HASHENTRY))
                SFREE(h->tab[i].left);
        }
    CFREE(h->tab, h->size, struct hashentry);
    TFREE(h, struct hashtable);
}


static inline void add_hash_value(struct hashtable *h, char *left, char *right)
{
    int i=hash(left)%h->size;
    while (h->tab[i].left)
    {
        if (!i)
            i=h->size-1;
        i--;
    }
    h->tab[i].left = left;
    h->tab[i].right= right;
}


static inline void rehash(struct hashtable *h, int s)
{
    struct hashentry *gt=h->tab;
    int gs=h->size;
    h->tab=CALLOC(s, struct hashentry);
    h->nent=h->nval;
    h->size=s;
    for (int i=0;i<gs;i++)
    {
        if (gt[i].left && gt[i].left!=DELETED_HASHENTRY)
            add_hash_value(h, gt[i].left, gt[i].right);
    }
    CFREE(gt, gs, struct hashentry);
}


/********************************************************************/
/* add a (key,value) pair to the hash table, rehashing if necessary */
/********************************************************************/
void set_hash(struct hashtable *h, const char *key, const char *value)
{
    if (h->nent*5 > h->size*4)
        rehash(h, h->nval*3);
    int i=hash(key)%h->size;
    while (h->tab[i].left)
    {
        if (h->tab[i].left==DELETED_HASHENTRY)
            goto found_tombstone;
        if (!strcmp(h->tab[i].left, key))
        {
            SFREE(h->tab[i].right);
            h->tab[i].right=mystrdup(value);
            return;
        }
        if (!i)
            i=h->size;
        i--;
    }
    h->nent++;
found_tombstone:
    h->tab[i].left = mystrdup(key);
    h->tab[i].right= mystrdup(value);
    h->nval++;
}


void set_hash_nostring(struct hashtable *h, const char *key, const char *value)
{
    if (h->nent*5 > h->size*4)
        rehash(h, h->nval*3);
    int i=hash(key)%h->size;
    while (h->tab[i].left)
    {
        if (h->tab[i].left==DELETED_HASHENTRY)
            goto found_tombstone;
        if (!strcmp(h->tab[i].left, key))
        {
            h->tab[i].right=(char*)value;
            return;
        }
        if (!i)
            i=h->size;
        i--;
    }
    h->nent++;
found_tombstone:
    h->tab[i].left = mystrdup(key);
    h->tab[i].right= (char*)value;
    h->nval++;
}


/****************************************************/
/* get the value for a given key, or 0 if not found */
/****************************************************/
char* get_hash(struct hashtable *h, const char *key)
{
    int i=hash(key)%h->size;
    while (h->tab[i].left)
    {
        if (h->tab[i].left!=DELETED_HASHENTRY&&(!strcmp(h->tab[i].left, key)))
        {
            return h->tab[i].right;
        }
        if (!i)
            i=h->size;
        i--;
    }
    return 0;
}


/****************************************************/
/* delete the key and its value from the hash table */
/****************************************************/
bool delete_hash(struct hashtable *h, const char *key)
{
    int i=hash(key)%h->size;
    while (h->tab[i].left)
    {
        if (h->tab[i].left!=DELETED_HASHENTRY&&(!strcmp(h->tab[i].left, key)))
        {
            SFREE(h->tab[i].left);
            SFREE(h->tab[i].right);
            h->tab[i].left=DELETED_HASHENTRY;
            h->nval--;
            if (h->nval*5<h->size)
                rehash(h, h->size/2);
            return true;
        }
        if (!i)
            i=h->size;
        i--;
    }
    return false;
}


/**************************************************************************/
/* create a sorted llist containing all entries of the table matching pat */
/**************************************************************************/
/* Rationale: hash tables are by definition unsorted.  When listing or    */
/* deleting from a list, we should show the entries in a sorted order,    */
/* however screen output is slow anyways, so we can sort it on the fly.   */
/**************************************************************************/
static int paircmp(const void *a, const void *b)
{
    return strcmp(((struct pair*)a)->left, ((struct pair*)b)->left);
}

typedef int (*compar_t)(const void*, const void*);

struct pairlist* hash2list(struct hashtable *h, const char *pat)
{
    int n = h->nval;
    auto pl = new struct pairlist[n*2+1];
    struct pair *p = &pl->pairs[0];

    for (int i=0;i<h->size;i++)
        if (h->tab[i].left && (h->tab[i].left!=DELETED_HASHENTRY)
            && (!pat || match(pat, h->tab[i].left)))
        {
            p->left = h->tab[i].left;
            p->right= h->tab[i].right;
            p++;
        }

    n = p-&pl->pairs[0];
    qsort(&pl->pairs[0], n, sizeof(struct pair), paircmp);
    pl->size = n;

    return pl;
}


/**************************/
/* duplicate a hash table */
/**************************/
struct hashtable* copy_hash(struct hashtable *h)
{
    struct hashtable *g=init_hash();
    CFREE(g->tab, g->size, struct hashentry);
    g->size=(h->nval>4) ? (h->nval*2) : 8;
    g->tab=CALLOC(g->size, struct hashentry);

    for (int i=0; i<h->size; i++)
        if (h->tab[i].left && h->tab[i].left!=DELETED_HASHENTRY)
            set_hash(g, h->tab[i].left, h->tab[i].right);
    return g;
}
