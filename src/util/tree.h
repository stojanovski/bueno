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

#ifndef _TREE_H_
#define _TREE_H_

/* inspired by linux kernel's container_of() macro */
#define get_container(type, member, ptr) \
    ((type *)(((char *)(ptr)) - ((char *)&(((type *)0)->member))))

typedef struct _bintree_node_t
{
    struct _bintree_node_t *parent;
    struct _bintree_node_t *left;
    struct _bintree_node_t *right;
    unsigned char color;
} bintree_node_t;

typedef struct _bintree_root_t
{
    bintree_node_t *node;
    size_t size;
} bintree_root_t;

void bintree_init(bintree_root_t *root);
void bintree_attach(bintree_node_t **leaf,
                    bintree_node_t *parent,
                    bintree_node_t *node);
void bintree_balance(bintree_root_t *root,
                     bintree_node_t *node);
void bintree_remove(bintree_root_t *root,
                    bintree_node_t *node);
size_t bintree_size(bintree_root_t *root);
void bintree_clear(bintree_root_t *root,
                   void (* free_func)(bintree_node_t *));

/** @param less_then_comparator a user-supplied function which returns true if
 * the left side is less then the right side; if duplicate values are allowed,
 * then the the function should return true if the left side is less than and
 * equal to the right side */
int __bintree_validate(bintree_root_t *root,
                       int (* less_then_comparator)(const bintree_node_t *,
                                                    const bintree_node_t *));

#endif  /* _TREE_H_ */
