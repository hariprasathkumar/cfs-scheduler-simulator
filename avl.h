
#ifndef _AVL_H
#define _AVL_H
#include "map.h"
struct task 
{
    long long vmruntime;
    long long remaining_time;
    long long pid;
    long long height;
    struct task *left;
    struct task *right;
};


void avl_print_tree(struct task *root);
struct task *avl_find_min(struct task *root);
struct task *avl_insert(struct task *root, struct task *node);
struct task *avl_delete(struct task *root, struct task **bubbled_node, long long pid, long long vmruntime) ;
struct task *avl_find_by_pid(struct task *root, long long pid);

#endif