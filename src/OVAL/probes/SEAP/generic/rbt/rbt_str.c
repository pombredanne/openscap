/*
 * Copyright 2010 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "Daniel Kopecek" <dkopecek@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assume.h>

#include "rbt_common.h"
#include "rbt_str.h"

static struct rbt_node *rbt_str_node_alloc(void)
{
        struct rbt_node *n = NULL;

#ifndef _WIN32
        if (posix_memalign((void **)(void *)(&n), sizeof(void *),
                           sizeof (struct rbt_node) + sizeof (struct rbt_str_node)) != 0)
        {
                abort ();
        }
#else
        // https://msdn.microsoft.com/en-us/library/8z34s9c6.aspx
        n = _aligned_malloc(sizeof (struct rbt_node) + sizeof (struct rbt_str_node), sizeof(void *));
#endif

        n->_chld[0] = NULL;
        n->_chld[1] = NULL;

        return(n);
}

static void rbt_str_node_free(struct rbt_node *n)
{
        if (n != NULL)
                free(rbt_node_ptr(n));
}

rbt_t *rbt_str_new (void)
{
        return rbt_new (RBT_STRKEY);
}

static void rbt_str_free_callback(struct rbt_str_node *n)
{
        free(n->key);
        /* node memory is freed by rbt_free */
}

void rbt_str_free (rbt_t *rbt)
{
        rbt_free(rbt, (void(*)(void *))&rbt_str_free_callback);
}

void rbt_str_free_cb (rbt_t *rbt, void (*callback)(struct rbt_str_node *))
{
        rbt_free(rbt, (void(*)(void *))callback);
}

void rbt_str_free_cb2 (rbt_t *rbt, void (*callback)(struct rbt_str_node *, void *user), void *user)
{
	rbt_free2(rbt, (void(*)(void *, void *))callback, user);
}

int rbt_str_add(rbt_t *rbt, char *key, void *data)
{
        struct rbt_node fake;
        struct rbt_node *h[4];
        register uint8_t dvec;
        register char *n_key, *u_key;
        register int  cmp;

        u_key = key;
        rbt_wlock(rbt);

        /*
         * Fake node
         */
        fake._chld[RBT_NODE_SL] = NULL;
        fake._chld[RBT_NODE_SR] = rbt->root;
        rbt_node_setcolor(&fake, RBT_NODE_CB);

        /*
         * Prepare node history stack & direction history vector
         */
        h[3] = NULL;
        h[2] = NULL;
        h[1] = &fake;
        h[0] = rbt->root;
        dvec = RBT_NODE_SR;

        for (;;) {
                if (rbt_node_ptr(h[0]) == NULL) {
                        /*
                         * Allocate new node aligned to sizeof(void *) bytes.
                         * On most systems, malloc already returns aligned
                         * memory but we want to ensure that its aligned using
                         * posix_memalign(3).
                         */
                        h[0] = rbt_str_node_alloc ();

                        rbt_str_node(h[0])->key = key;
                        rbt_str_node(h[0])->data = data;

                        /*
                         * Set the new node as the child of the last node we've
                         * visited. The last direction is the lowest bit of the
                         * direction history vector.
                         */
                        rbt_node_setptr(rbt_node_ptr(h[1])->_chld[dvec & 1], h[0]);
                        rbt_node_setcolor(h[0], RBT_NODE_CR);

                        /*
                         * Since we are inserting a new red node, we need to fix
                         * a red violation if the parent of the new node is red.
                         */
                        if (rbt_node_getcolor(h[1]) == RBT_NODE_CR) {
                                rbt_redfix(h, dvec, rbt_node_ptr(h[3])->_chld[(dvec >> 2) & 1]);
                        }

                        /*
                         * Update root node and color it black
                         */
                        rbt->root = fake._chld[RBT_NODE_SR];
                        rbt_node_setcolor(rbt->root, RBT_NODE_CB);

                        /*
                         * Update node counter
                         */
                        ++(rbt->size);

                        break;
                } else if (rbt_node_getcolor(rbt_node_ptr(h[0])->_chld[0]) == RBT_NODE_CR &&
                           rbt_node_getcolor(rbt_node_ptr(h[0])->_chld[1]) == RBT_NODE_CR)
                {
                        /*
                         * Color switch
                         */
                        rbt_node_setcolor(h[0], RBT_NODE_CR);
                        rbt_node_setcolor(rbt_node_ptr(h[0])->_chld[0], RBT_NODE_CB);
                        rbt_node_setcolor(rbt_node_ptr(h[0])->_chld[1], RBT_NODE_CB);

                        /*
                         * Fix red violation
                         */
                        if (rbt_node_getcolor(h[1]) == RBT_NODE_CR) {
                                rbt_redfix(h, dvec, rbt_node_ptr(h[3])->_chld[(dvec >> 2) & 1]);
                        }
                } else if (rbt_node_getcolor(h[0]) == RBT_NODE_CR &&
                           rbt_node_getcolor(h[1]) == RBT_NODE_CR)
                {
                        /*
                         * Fix red violation
                         */
                        rbt_redfix(h, dvec, rbt_node_ptr(h[3])->_chld[(dvec >> 2)&1]);
                }

                n_key = rbt_str_node(h[0])->key;
                cmp   = strcmp(u_key, n_key);

                if (cmp < 0) {
                        rbt_hpush4(h, rbt_node_ptr(h[0])->_chld[RBT_NODE_SL]);
                        dvec = (dvec << 1) | RBT_NODE_SL;
                } else if (cmp > 0) {
                        rbt_hpush4(h, rbt_node_ptr(h[0])->_chld[RBT_NODE_SR]);
                        dvec = (dvec << 1) | RBT_NODE_SR;
                } else {
                        /*
                         * Update root node and color it black
                         */
                        rbt->root = fake._chld[RBT_NODE_SR];
                        rbt_node_setcolor(rbt->root, RBT_NODE_CB);

                        rbt_wunlock(rbt);
                        return (1);
                }
        }

        rbt_wunlock(rbt);
        return (0);
}

void *rbt_str_rep(rbt_t *rbt, const char *key, void *data)
{
        return (NULL);
}

int rbt_str_del(rbt_t *rbt, const char *key, void **n)
{
        struct rbt_node fake, *save;
        struct rbt_node *h[4];
        register uint8_t dvec;
        register char *n_key, *u_key;
        register int cmp;

        save  = NULL;
        u_key = (char *)key;

        rbt_wlock(rbt);

        if (rbt->size == 0) {
                rbt_wunlock(rbt);
                return (1);
        }

        assume_d(rbt_node_ptr(rbt->root) != NULL, -1);

        /*
         * Fake node
         */
        fake._chld[RBT_NODE_SL] = NULL;
        fake._chld[RBT_NODE_SR] = rbt->root;
        rbt_node_setcolor(&fake, RBT_NODE_CB);

        /*
         * Prepare node history stack & direction history vector
         */
        h[2] = NULL;
        h[1] = NULL;
        h[0] = &fake;
        dvec = RBT_NODE_SR;

        for (;;) {
                if (rbt_node_ptr(rbt_node_ptr(h[0])->_chld[dvec & 1]) == NULL)
                        break;

                rbt_hpush4(h, rbt_node_ptr(h[0])->_chld[dvec & 1]);
                n_key = rbt_str_node(h[0])->key;
                cmp   = strcmp(u_key, n_key);

                if (cmp < 0) {
                        dvec = (dvec << 1) | RBT_NODE_SL;
                } else if (cmp > 0) {
                        dvec = (dvec << 1) | RBT_NODE_SR;
                } else {
                        save = rbt_node_ptr(h[0]);
                        dvec = (dvec << 1) | RBT_NODE_SL;
                }

                if (rbt_node_getcolor(h[0]) == RBT_NODE_CB &&
                    rbt_node_getcolor(rbt_node_ptr(h[0])->_chld[dvec & 1]) == RBT_NODE_CB)
                {
                        if (rbt_node_getcolor(rbt_node_ptr(h[0])->_chld[!(dvec & 1)]) == RBT_NODE_CR) {
                                switch(dvec & 1) {
                                case RBT_NODE_SR:
                                        rbt_node_setptr(rbt_node_ptr(h[1])->_chld[(dvec >> 1) & 1],
                                                        rbt_node_rotate_R(h[0]));
                                        break;
                                case RBT_NODE_SL:
                                        rbt_node_setptr(rbt_node_ptr(h[1])->_chld[(dvec >> 1) & 1],
                                                        rbt_node_rotate_L(h[0]));
                                        break;
                                }

                                h[1] = rbt_node_ptr(h[1])->_chld[(dvec >> 1) & 1];
                        } else {
                                register struct rbt_node *s;

                                s = rbt_node_ptr(rbt_node_ptr(h[1])->_chld[!((dvec >> 1) & 1)]);

                                if (s != NULL) {
                                        if (rbt_node_getcolor(s->_chld[0]) == RBT_NODE_CB &&
                                            rbt_node_getcolor(s->_chld[1]) == RBT_NODE_CB)
                                        {
                                                rbt_node_setcolor(h[0], RBT_NODE_CR);
                                                rbt_node_setcolor(h[1], RBT_NODE_CB);
                                                rbt_node_setcolor(s,    RBT_NODE_CR);
                                        } else {
                                                register uint8_t d = rbt_node_ptr(rbt_node_ptr(h[2])->_chld[RBT_NODE_SR]) == rbt_node_ptr(h[1]);

                                                if (rbt_node_getcolor(s->_chld[(dvec >> 1) & 1]) == RBT_NODE_CR) {
                                                        switch((dvec >> 1) & 1) {
                                                        case RBT_NODE_SR:
                                                                rbt_node_setptr(rbt_node_ptr(h[2])->_chld[d],
                                                                                rbt_node_rotate_RL(h[1]));
                                                                break;
                                                        case RBT_NODE_SL:
                                                                rbt_node_setptr(rbt_node_ptr(h[2])->_chld[d],
                                                                                rbt_node_rotate_LR(h[1]));
                                                                break;
                                                        }
                                                } else if (rbt_node_getcolor(s->_chld[!((dvec >> 1) & 1)]) == RBT_NODE_CR) {
                                                        switch((dvec >> 1) & 1) {
                                                        case RBT_NODE_SR:
                                                                rbt_node_setptr(rbt_node_ptr(h[2])->_chld[d],
                                                                                rbt_node_rotate_R(h[1]));
                                                                break;
                                                        case RBT_NODE_SL:
                                                                rbt_node_setptr(rbt_node_ptr(h[2])->_chld[d],
                                                                                rbt_node_rotate_L(h[1]));
                                                                break;
                                                        }
                                                }

                                                s = rbt_node_ptr(rbt_node_ptr(h[2])->_chld[d]);

                                                rbt_node_setcolor(h[0], RBT_NODE_CR);
                                                rbt_node_setcolor(s,    RBT_NODE_CR);
                                                rbt_node_setcolor(s->_chld[0], RBT_NODE_CB);
                                                rbt_node_setcolor(s->_chld[1], RBT_NODE_CB);
                                        }
                                }
                        }
                }
        }

        if (save != NULL) {
                h[0] = rbt_node_ptr(h[0]);
                /*
                 * The node color of the node that will be delete is always
                 * red in case the node is not the root node.
                 */
                assume_d(rbt_node_ptr(fake._chld[RBT_NODE_SR]) == h[0] || rbt_node_getcolor(h[0]) == RBT_NODE_CR, -1);
                if (n != NULL)
                        *n = rbt_str_node(save)->data;

                rbt_str_node(save)->data = rbt_str_node(h[0])->data;
                rbt_str_node(save)->key  = rbt_str_node(h[0])->key;

                dvec = rbt_node_ptr(rbt_node_ptr(h[1])->_chld[1]) == h[0];

                if (rbt_node_ptr(h[0]->_chld[RBT_NODE_SR]) == NULL) {
                        rbt_node_setptr(rbt_node_ptr(h[1])->_chld[dvec], h[0]->_chld[RBT_NODE_SL]);
                } else {
                        rbt_node_setptr(rbt_node_ptr(h[1])->_chld[dvec], h[0]->_chld[RBT_NODE_SR]);
                }

                /*
                 * Free memory used by the deleted node and decrement
                 * the node counter.
                 */
                rbt_str_node_free(h[0]);
                --(rbt->size);
        }

        /*
         * Update root node and color it black and unlock the tree.
         */
        rbt->root = fake._chld[RBT_NODE_SR];
        rbt_node_setcolor(rbt->root, RBT_NODE_CB);
        rbt_wunlock(rbt);

        return (save == NULL ? 1 : 0);
}

int rbt_str_getnode(rbt_t *rbt, const char *key, struct rbt_str_node **node)
{
        int r;
        register struct rbt_node *n;
        register char *u_key, *n_key;
        register int   cmp;

        u_key = (char *)key;
        r = -1;
        rbt_rlock(rbt);
        n = rbt_node_ptr(rbt->root);

        while (n != NULL) {
                n_key = rbt_str_node(n)->key;
                cmp   = strcmp(u_key, n_key);

                if (cmp < 0)
                        n = rbt_node_ptr(n->_chld[RBT_NODE_SL]);
                else if (cmp > 0)
                        n = rbt_node_ptr(n->_chld[RBT_NODE_SR]);
                else {
                        r = 0;
                        *node = (struct rbt_str_node *)(n->_node);
                        break;
                }
        }

        rbt_runlock(rbt);
        return (r);
}

int rbt_str_get(rbt_t *rbt, const char *key, void **data)
{
        int r;
        struct rbt_str_node *n = NULL;

        r = rbt_str_getnode(rbt, key, &n);

        if (r == 0)
                *data = n->data;

        return(r);
}

int rbt_str_walk_preorder(rbt_t *rbt, int (*callback)(struct rbt_str_node *), rbt_walk_t flags)
{
        errno = ENOSYS;
        return (-1);
}

int rbt_str_walk_inorder(rbt_t *rbt, int (*callback)(struct rbt_str_node *), rbt_walk_t flags)
{
        return rbt_walk_inorder(rbt, (int(*)(void *))callback, flags);
}

int rbt_str_walk_inorder2(rbt_t *rbt, int (*callback)(struct rbt_str_node *, void *), void *user, rbt_walk_t flags)
{
        return rbt_walk_inorder2(rbt, (int(*)(void *, void *))callback, user, flags);
}

int rbt_str_walk_postorder(rbt_t *rbt, int (*callback)(struct rbt_str_node *), rbt_walk_t flags)
{
        errno = ENOSYS;
        return (-1);
}

int rbt_str_walk_levelorder(rbt_t *rbt, int (*callback)(struct rbt_str_node *), rbt_walk_t flags)
{
        errno = ENOSYS;
        return (-1);
}

int rbt_str_walk(rbt_t *rbt, rbt_walk_t type, int (*callback)(struct rbt_str_node *))
{
	if (rbt == NULL || callback == NULL) {
		errno = EINVAL;
		return -1;
	}

        switch (type & RBT_WALK_TYPEMASK) {
        case RBT_WALK_PREORDER:   return rbt_str_walk_preorder(rbt, callback, type);
        case RBT_WALK_INORDER:    return rbt_str_walk_inorder(rbt, callback, type);
        case RBT_WALK_POSTORDER:  return rbt_str_walk_postorder(rbt, callback, type);
        case RBT_WALK_LEVELORDER: return rbt_str_walk_levelorder(rbt, callback, type);
        }

        errno = EINVAL;
        return (-1);
}

size_t rbt_str_size(rbt_t *rbt)
{
        return rbt_size(rbt);
}
