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
static void update_parents(bintree_node_t *orig_parent,
                           const bintree_node_t * const orig,
                           bintree_node_t * const newnode)
{
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
    bintree_node_t *first_parent = first->parent;
    bintree_node_t *second_parent = second->parent;
    update_parents(first_parent, first, second);
    update_parents(second_parent, second, first);
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
    second->color = first->color;
}

/* find the successor value node, then swap these two nodes */
static void swap_with_successor(bintree_node_t *node)
{
    bintree_node_t *right_min;
    assert(node != NULL);
    assert(node->right != NULL);

    /* find the successor node */
    right_min = node->right;
    while (right_min->left != NULL)
        right_min = right_min->left;

    swap_nodes(node, right_min);
}


static void remove_node(bintree_node_t *node)
{
    /* if the node to remove has two children, reduce this first to a problem
     * of removing a one or a zero child node: replace such node with the
     * successor value node, which is guaranteed to be not have two children */
    if (node->left != NULL && node->right != NULL)
        swap_with_successor(node);

    /* at this point, must only have zero or one child */
    assert(!(node->left != NULL && node->right != NULL));
}
