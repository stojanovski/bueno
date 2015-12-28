/*
 * Copyright 2015 Igor Stojanovski
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tree.h"
#include <stdio.h>
#include <assert.h>

#define RED 0
#define BLACK 1

void bintree_init(bintree_root_t *root)
{
    root->node = NULL;
}

static bintree_node_t *grandparent(bintree_node_t *node)
{
    if ((node != NULL) && (node->parent != NULL))
        return node->parent->parent;
    else
        return NULL;
}

static void rotate_right(bintree_node_t *parent)
{
    bintree_node_t *grandparent, *orig_left;
    assert(parent != NULL);

    grandparent = parent->parent;
    assert(grandparent != NULL);
    orig_left = parent->left;
    assert(orig_left != NULL);

    parent->parent = orig_left;
    parent->left = orig_left->right;

    if (grandparent->left == parent)
        grandparent->left = orig_left;
    else
        grandparent->right = orig_left;

    orig_left->right = parent;
    orig_left->parent = grandparent;
}

static void rotate_left(bintree_node_t *parent)
{
    bintree_node_t *grandparent, *orig_right;
    assert(parent != NULL);

    grandparent = parent->parent;
    assert(grandparent != NULL);
    orig_right = parent->right;
    assert(orig_right != NULL);

    parent->parent = orig_right;
    parent->right = orig_right->left;

    if (grandparent->left == parent)
        grandparent->left = orig_right;
    else
        grandparent->right = orig_right;

    orig_right->left = parent;
    orig_right->parent = grandparent;
}

/* algorithm based on https://en.wikipedia.org/wiki/Red%E2%80%93black_tree */
static void rebalance_after_insert(bintree_node_t *node)
{
    bintree_node_t *uncle, *grandparent;
start:
    if (node->parent == NULL) {
        /* empty tree; root node must be black: repaint ("Case 1") */
        node->color = BLACK;
        return;
    }
    else if (node->parent->color == BLACK) {
        /* if parent is black, axiomatically tree is balanced ("Case 2") */
        return;
    }

    assert(node->parent != NULL);

    grandparent = node->parent->parent;
    assert(grandparent != NULL);

    uncle = (grandparent->left == node->parent) ? grandparent->right :
                                                  grandparent->left;

    /* "Case 3" */
    if (uncle != NULL && uncle->color == RED) {
        /* if uncle is red: paint parent and uncle red, grandparent black */
        node->parent->color = uncle->color = BLACK;
        uncle->parent->color = RED;

        /* then redo all this starting from the grandparent */
        node = grandparent;
        goto start;
    }

    assert(grandparent != NULL);

    /* Handle: parent is red, uncle is black, node is right (left) child of
     * parent, parent is left (right) child of grandparent.  ("Case 4") */
    if (node->parent == grandparent->left && node == node->parent->right) {
        /* reverse the roles of the child and the parent nodes */
        rotate_left(node->parent);

        grandparent = node->parent;
        node = node->left;
    }
    else if (node->parent == grandparent->right && node == node->parent->left) {
        /* reverse the roles of the child and the parent nodes */
        rotate_right(node->parent);

        grandparent = node->parent;
        node = node->right;
    }

    assert(grandparent != NULL);
    assert(grandparent->color = BLACK);
    assert(node->parent != NULL);
    assert(node->parent->color = RED);

    /* The parent is red, but the uncle is black; node is left (right) of
     * parent, parent is left (right) of grandparent ("Case 5") */
    grandparent->color = RED;
    node->parent->color = BLACK;
    if (node == node->parent->left)
        rotate_right(grandparent);
    else
        rotate_left(grandparent);
}

void bintree_balance(bintree_root_t *root, bintree_node_t *node)
{
}

static void swap_node_pointers(bintree_node_t **first,
                               bintree_node_t **second)
{
    bintree_node_t *tmp_node = *first;
    *first = *second;
    *second = tmp_node;
}

/* set parent child link to newnode */
static void update_parents(const bintree_node_t * const orig,
                           bintree_node_t * const newnode)
{
    bintree_node_t *orig_parent = orig->parent;
    if (orig_parent != NULL) {
        if (orig == orig_parent->left)
            orig_parent->left = newnode;
        else {
            assert(orig == orig_parent->right);
            orig_parent->right = newnode;
        }
    }
}

/* swap two nodes' parent child link */
static void swap_parent_child_pointers(bintree_node_t *first,
                                       bintree_node_t *second)
{
    update_parents(first, second);
    update_parents(second, first);
}

/* swap two nodes' positions wihtin a tree */
static void swap_nodes(bintree_node_t *first, bintree_node_t *second)
{
    unsigned char tmp_color;
    assert(first != NULL);
    assert(second != NULL);
    assert(first != second);

    swap_parent_child_pointers(first, second);

    swap_node_pointers(&first->parent, &second->parent);
    swap_node_pointers(&first->left, &second->left);
    swap_node_pointers(&first->right, &second->right);

    tmp_color = first->color;
    first->color = second->color;
    second->color = tmp_color;
}

/* find the successor value node, then swap these two nodes */
static void swap_with_successor(bintree_node_t *node)
{
    bintree_node_t *right_min;
    assert(node->right != NULL);

    /* find the successor node */
    right_min = node->right;
    while (right_min->left != NULL)
        right_min = right_min->left;

    swap_nodes(node, right_min);
}

static void detach_node(bintree_node_t *node)
{
    bintree_node_t *parent = node->parent;
    if (parent != NULL) {
        if (node == parent->left)
            parent->left = NULL;
        else {
            assert(node == parent->right);
            parent->right = NULL;
        }
    }
}

/* place dst in place of src in the tree; src is detached from the tree and
 * should be freed; src *must* be dst's parent! */
static void replace_node(bintree_node_t *dst, bintree_node_t *src)
{
    bintree_node_t *src_parent = src->parent;
    assert(src->left == NULL && src->right == NULL);
    assert(src == dst->parent);  /* src *must* be dst's parent! */

    /* update parent's left/right to the destination node */
    if (src_parent != NULL) {
        if (src_parent->left == src)
            src_parent->left = dst;
        else {
            assert(src_parent->right == src);
            src_parent->right = dst;
        }
    }

    /* fit dst node into src's tree location */
    dst->parent = src_parent;
    dst->left = src->left;
    dst->right = src->right;
}

static int has_no_children(const bintree_node_t *node)
{
    return node->left == NULL && node->right == NULL;
}

static int has_one_child(const bintree_node_t *node)
{
    return (node->left != NULL) ? (node->right == NULL) :
                                  (node->right != NULL);
}

static int has_both_children(const bintree_node_t *node)
{
    return node->left != NULL && node->right != NULL;
}

static bintree_node_t *get_the_one_child(bintree_node_t *node)
{
    assert(has_one_child(node));
    return node->left != NULL ? node->left : node->right;
}

/* sibling_on_right is set to true if the sibling is to the right of the node's
 * parent, false if it is to the left; sibling_on_right is not set if there is
 * no sibling */
static bintree_node_t *sibling(bintree_node_t *node, int *sibling_on_right)
{
    bintree_node_t *parent = node->parent;
    if (parent) {
        if (parent->left == node) {
            *sibling_on_right = 1;
            return parent->right;
        }
        else {
            assert(parent->right == node);
            *sibling_on_right = 0;
            return parent->left;
        }
    }
    return NULL;
}

/* algorithm based on https://en.wikipedia.org/wiki/Red%E2%80%93black_tree */
static void remove_black_leaf(bintree_node_t *node)
{
    bintree_node_t *sib;
    int sib_on_right;
start_over:
    /* if tree has a single node, then simply remove it ("Case 1") */
    if (node->parent == NULL)
        return;

    /* sibling is red: reverse the colors of the parent and the sibling;
     * sibling to the right (left) of the parent should be rotated
     * left (right), such that the sibling becomes the grandparent of the
     * node ("Case 2") */
    sib = sibling(node, &sib_on_right);
    if (sib != NULL && sib->color == RED) {
        assert(node->parent->color == BLACK);
        node->parent->color = RED;
        sib->color = BLACK;
        if (sib_on_right)
            rotate_left(node->parent);
        else
            rotate_right(node->parent);
    }

    /* if the parent, the sibling and the sibling's children are black,
     * repaint sibling to red and start over from above using the parent
     * as the new node ("Case 3") */
    sib = sibling(node, &sib_on_right);
    assert(node->parent != NULL);
    if (sib != NULL &&
        sib->color == BLACK &&
        node->parent->color == BLACK &&
        sib->left != NULL && sib->left->color == BLACK &&
        sib->right != NULL && sib->right->color == BLACK)
    {
        sib->color = RED;
        node = node->parent;
        goto start_over;
    }

    /* if the sibling and its children are black but the parent is red,
     * exchange the colors of the sibling and the parent ("Case 4") */
    sib = sibling(node, &sib_on_right);
    if (sib != NULL &&
        sib->color == BLACK &&
        node->parent->color == RED &&
        sib->left != NULL && sib->left->color == BLACK &&
        sib->right != NULL && sib->right->color == BLACK)
    {
        sib->color = RED;
        node->parent->color = BLACK;
        return;
    }

    /* if sibling is black, its left (right) child is red, its right (left)
     * child is black, and sibling is right (left) child of its parent, rotate
     * right (left) at sibling; exchange colors of the sibling and its new
     * parent ("Case 5") */
    if (sib != NULL &&
        sib->color == BLACK)
    {
        if (sib_on_right &&
            sib->left != NULL && sib->left->color == RED &&
            sib->right != NULL && sib->right->color == BLACK)
        {
            sib->color = RED;
            sib->left->color = BLACK;
            rotate_right(sib);
        }
        else if (!sib_on_right &&
            sib->left != NULL && sib->left->color == BLACK &&
            sib->right != NULL && sib->right->color == RED)
        {
            sib->color = RED;
            sib->right->color = BLACK;
            rotate_left(sib);
        }
    }

    /* sibling is right (left) child of its parent and is black, its
     * right (left) child is red; we rotate left (right) at parent; exchange
     * colors of parent and sibling and make sibling's right (left) child
     * black ("Case 6") */
    sib = sibling(node, &sib_on_right);
    if (sib != NULL &&
        sib->color == BLACK)
    {
        sib->color = sib->parent->color;
        sib->parent->color = BLACK;
        if (sib_on_right) {
            sib->right->color = BLACK;
            rotate_left(node->parent);
        }
        else {
            sib->left->color = BLACK;
            rotate_right(node->parent);
        }
    }
}

static void remove_node(bintree_node_t *node)
{
    assert(node != NULL);

    /* if node is a red leaf, simply remove node as this won't violate any RB
     * tree rules */
    if (node->color == RED && has_no_children(node)) {
        assert(node->parent != NULL);  /* red node can't be a root */
        detach_node(node);
        return;
    }

    /* if node is a single-child parent, it must be black; replace with its
     * child, which must be red, and recolor that child black */
remove_one_child_node:
    if (has_one_child(node)) {
        bintree_node_t *child = get_the_one_child(node);
        assert(node->color == BLACK);
        assert(child->color == RED);
        replace_node(child, node);
        child->color = BLACK;
        return;
    }

    if (has_both_children(node)) {
        /* if the node to remove has two children, reduce this first to a
         * problem of removing a one or a zero child node: replace such node
         * with the successor value node, which is guaranteed to not have
         * two children */
        swap_with_successor(node);
        /* at this point, node is at former successor place in the tree,
         * including having the former successor color */

        if (node->color == RED) {
            assert(has_no_children(node));  /* must not have children */

            /* simply remove this red leaf node, which shouldn't violate any
             * RB tree rules */
            detach_node(node);
            return;
        }
        else if (has_one_child(node))
            goto remove_one_child_node;
    }

    /* the node is now a black leaf */
    assert(has_no_children(node));
    assert(node->color == BLACK);
    remove_black_leaf(node);
    detach_node(node);
}
