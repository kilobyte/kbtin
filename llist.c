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
    nexttodel = nptr->next;
    LFREE(nptr);

    for (nptr = nexttodel; nptr; nptr = nexttodel)
    {
        nexttodel = nptr->next;
        SFREE(nptr->left);
        SFREE(nptr->right);
        SFREE(nptr->pr);
        LFREE(nptr);
    }
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
    for (int i=0; i<MAX_PATH_LENGTH; i++)
        free((char*)ses->path[i].left), free((char*)ses->path[i].right);
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
    ses->binds = init_hash();
    ses->path_begin = ses->path_length = 0;
    bzero(ses->path, sizeof(ses->path));
    ses->pathdirs = init_hash();
#ifdef HAVE_HS
    ses->highs_dirty = true;
#endif
    tintin_printf(ses, "#Lists cleared.");
}

/*****************************/
/* delete a node from a list */
/*****************************/
void deletenode_list(struct listnode *listhead, struct listnode *nptr)
{
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
}

/*********************************************************************/
/* create a node containing the ltext, rtext fields and place at the */
/* end of a list - as insertnode_list(), but not alphabetical        */
/*********************************************************************/
void addnode_list(struct listnode *listhead, const char *ltext, const char *rtext, const char *prtext)
{
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
