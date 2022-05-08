/*********************************************************************/
/* file: llist.c - linked-list datastructure                         */
/*                             TINTIN III                            */
/*          (T)he K(I)cki(N) (T)ickin D(I)kumud Clie(N)t             */
/*                     coded by peter unold 1992                     */
/*********************************************************************/
#include "tintin.h"
#include "protos/events.h"
#include "protos/glob.h"
#include "protos/hash.h"
#include "protos/llist.h"
#include "protos/print.h"
#include "protos/routes.h"
#include "protos/slist.h"
#include "protos/tlist.h"
#include "protos/utils.h"
#include <assert.h>


/***************************************/
/* init list - return: ptr to listhead */
/***************************************/
struct listnode* init_list(void)
{
    struct listnode *listhead;

    if (!(listhead = TALLOC(struct listnode)))
        syserr("couldn't alloc listhead");
    listhead->next = NULL;
    listhead->right = 0;
    return listhead;
}

/***********************************************/
/* kill list - run through list and free nodes */
/***********************************************/
void kill_list(struct listnode *nptr)
{
    struct listnode *nexttodel;

    if (!nptr)
        syserr("NULL PTR");
    int len = LISTLEN(nptr);
    nexttodel = nptr->next;
    LFREE(nptr);

    for (nptr = nexttodel; nptr; nptr = nexttodel)
    {
        nexttodel = nptr->next;
        SFREE(nptr->left);
        SFREE(nptr->right);
        SFREE(nptr->pr);
        LFREE(nptr);
        len--;
    }
    assert(!len);
}


/***********************************************************/
/* zap only the list structure, without freeing the string */
/***********************************************************/
void zap_list(struct listnode *nptr)
{
    struct listnode *nexttodel;

    if (!nptr)
        syserr("NULL PTR");
    int len = LISTLEN(nptr);
    nexttodel = nptr->next;
    LFREE(nptr);

    for (nptr = nexttodel; nptr; nptr = nexttodel)
    {
        nexttodel = nptr->next;
        LFREE(nptr);
        len--;
    }
    assert(!len);
}


/********************************************************************
**   This function will clear all lists associated with a session  **
********************************************************************/
void kill_all(struct session *ses, bool no_reinit)
{
    if (!ses) // can't happen
        return;

    kill_hash(ses->aliases);
    kill_tlist(ses->actions);
    kill_tlist(ses->prompts);
    kill_hash(ses->myvars);
    kill_tlist(ses->highs);
    kill_tlist(ses->subs);
    kill_slist(ses->antisubs);
    kill_list(ses->path);
    kill_hash(ses->pathdirs);
    kill_hash(ses->binds);
    kill_routes(ses);
    kill_events(ses);
    if (no_reinit)
        return;

    ses->aliases = init_hash();
    ses->actions = init_tlist();
    ses->prompts = init_tlist();
    ses->myvars = init_hash();
    ses->highs = init_tlist();
    ses->subs = init_tlist();
    ses->antisubs = init_slist();
    ses->path = init_list();
    ses->binds = init_hash();
    ses->pathdirs = init_hash();
#ifdef HAVE_HS
    ses->highs_dirty = true;
#endif
    tintin_printf(ses, "#Lists cleared.");
}

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
    if (!a && !b)
        return 0;
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
int strlongercmp(const char *a, const char *b)
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

/*****************************/
/* delete a node from a list */
/*****************************/
void deletenode_list(struct listnode *listhead, struct listnode *nptr)
{
    LISTLEN(listhead)--;
    struct listnode *lastnode = listhead;

    while ((listhead = listhead->next))
    {
        if (listhead == nptr)
        {
            lastnode->next = listhead->next;
            SFREE(listhead->left);
            SFREE(listhead->right);
            SFREE(listhead->pr);
            LFREE(listhead);
            return;
        }
        lastnode = listhead;
    }
    return;
}

/*************************************/
/* show contents of a node on screen */
/*************************************/
static void shownode_list(struct listnode *nptr)
{
    tintin_printf(0, "~7~{%s~7~}={%s~7~}", nptr->left, nptr->right);
}

/*************************************/
/* list contents of a list on screen */
/*************************************/
void show_list(struct listnode *listhead)
{
    while ((listhead = listhead->next))
        shownode_list(listhead);
}

/*********************************************************************/
/* create a node containing the ltext, rtext fields and place at the */
/* end of a list - as insertnode_list(), but not alphabetical        */
/*********************************************************************/
void addnode_list(struct listnode *listhead, const char *ltext, const char *rtext, const char *prtext)
{
    LISTLEN(listhead)++;

    struct listnode *newnode;
    if (!(newnode = TALLOC(struct listnode)))
        syserr("couldn't malloc listhead");
    newnode->left = mystrdup(ltext);
    if (rtext)
        newnode->right = mystrdup(rtext);
    else
        newnode->right = 0;
    if (prtext)
        newnode->pr = mystrdup(prtext);
    else
        newnode->pr = 0;
    newnode->next = NULL;
    while (listhead->next)
        listhead = listhead->next;
    listhead->next = newnode;
}
