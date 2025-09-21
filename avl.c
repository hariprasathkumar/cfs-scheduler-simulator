#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include "avl.h"

static inline size_t max(long long a, long long b)
{
    return (a > b) ? a : b;
} 

// compare vmruntime + pid as vmruntime maybe same
static int compare(long long v1, long long p1, long long v2, long long p2)
{
    if (v1 < v2) return -1;
    if (v1 > v2) return +1;
    if (p1 < p2) return -1;
    if (p1 > p2) return +1;
    return 0;
}

static long long get_height(struct task *node) 
{
    return !node ? 0 : node->height;
}

static int balance_factor(struct task *node) 
{
    if (!node) return 0;

    return get_height(node->left) - get_height(node->right);
}
/* Right rotate subtree rooted at x
 *
 *       x                 y
 *      / \               / \
 *     y   T3   --->     T1  x
 *    / \                   / \
 *   T1  T2                T2  T3
 */
static struct task *right_rotate(struct task *node) 
{
    struct task *y  = node->left;
    struct task *T2 = y->right;

    // Perform rotation, reversing the order will create a cycle !!
    node->left = T2;
    y->right   = node;

    // Update heights
    node->height = 1 + max(get_height(node->left), get_height(node->right));
    y->height    = 1 + max(get_height(y->left), get_height(y->right));

    return y;
}
/* Left rotate subtree rooted at x
 *
 *    x                    y
 *   / \                  / \
 *  T1  y     --->       x  T3
 *     / \              / \
 *    T2  T3           T1 T2
 */
static struct task *left_rotate(struct task *node) 
{
    struct task *y  = node->right;
    struct task *T2 = y->left;

    // Perform rotation, reversing the order will create a cycle !!
    node->right = T2;
    y->left     = node;

    // Update heights
    node->height = 1 + max(get_height(node->left), get_height(node->right));
    y->height    = 1 + max(get_height(y->left), get_height(y->right));

    return y;
}

void avl_print_tree(struct task *root)
{
    if (!root) return;

    avl_print_tree(root->left);
    #ifdef DEBUG
    fprintf(stdout, "%lld %lld %lld\n", root->pid, root->vmruntime, root->remaining_time);
    #endif
    avl_print_tree(root->right);
}

struct task *avl_find_min(struct task *root) 
{
    if (!root) return NULL;
    if (!root->left) return root;

    return avl_find_min(root->left);
}

struct task *avl_find_by_pid(struct task *root, long long pid) 
{
    struct task *stack[1024];
    int top = 0;

    if (root) stack[top++] = root;

    while (top > 0) {
        struct task *cur = stack[--top];

        if (cur->pid == pid)
            return cur;

        if (cur->right) stack[top++] = cur->right;
        if (cur->left)  stack[top++] = cur->left;
    }

    return NULL;
}

struct task *avl_insert(struct task *root, struct task *node) 
{
    if (!root) 
    {  
        node->height = 1;
        node->left = node->right = NULL;
        return node;
    }

    int cmp  = compare(node->vmruntime, node->pid, root->vmruntime, root->pid);
    if (cmp < 0) root->left = avl_insert(root->left, node);
    else if (cmp > 0) root->right = avl_insert(root->right, node);

    root->height = 1 + max(get_height(root->left), get_height(root->right));

    int bf_cur   = balance_factor(root);

    /* 
                x
            y
        z
    */
    /*
            x
        y
            z
    */
    if (bf_cur > 1 && root->left) // LL
    {
        if (compare(node->vmruntime, node->pid, root->left->vmruntime, root->left->pid) < 0)
            root = right_rotate(root);
        else if (compare(node->vmruntime, node->pid, root->left->vmruntime, root->left->pid) > 0)
        {
            root->left = left_rotate(root->left);
            root = right_rotate(root);
        }
    }
    /*
        x
            y
                z
    */
    /*
        x
            y
        z
    */
    else if (bf_cur < -1 && root->right) // RR
    {
        if (compare(node->vmruntime, node->pid, root->right->vmruntime, root->right->pid) > 0)
        root = left_rotate(root);
        else if (compare(node->vmruntime, node->pid, root->right->vmruntime, root->right->pid) < 0)
        {
            root->right = right_rotate(root->right);
            root = left_rotate(root);
        }
    }

    return root;
}

struct task *avl_delete(struct task *root, struct task **bubbled_node, long long pid, long long vmruntime) 
{
    if (root == NULL) return root;

    struct task *ret = NULL;
    struct task *replace = NULL;

    int cmp = compare(vmruntime, pid, root->vmruntime, root->pid);
    if (cmp < 0) root->left = avl_delete(root->left, bubbled_node, pid, vmruntime);
    else if (cmp > 0) root->right = avl_delete(root->right, bubbled_node, pid, vmruntime);

    if (!root) return NULL;

    if (cmp == 0)
    {
        if (root->left && root->right) 
        {
            replace = avl_find_min(root->right);
            
            root->pid = replace->pid;
            root->vmruntime = replace->vmruntime;
            root->remaining_time = replace->remaining_time;

            root->right = avl_delete(root->right, bubbled_node, replace->pid, replace->vmruntime);
        } 
        else if (root->left) 
        {
            replace = root->left;
            *bubbled_node = root;
            root = replace;
        } 
        else if (root->right) 
        {
            replace = root->right;
            *bubbled_node = root;
            root = replace;
        } 
        else
        {
            *bubbled_node = root;
            return NULL;
        }
    }

    root->height = 1 + max(get_height(root->left), get_height(root->right));

    int bf_cur   = balance_factor(root);
    int bf_left  = balance_factor(root->left);
    int bf_right = balance_factor(root->right);
    
    // LL
    if (bf_cur > 1 && bf_left >= 0) 
    {
        // Right ROtate
        root = right_rotate(root);
    } 
    else if (bf_cur < -1 && bf_right <= 0) 
    { // RR
        // Left Rotate
        root = left_rotate(root);
    } 
    else if (bf_cur > 1 && bf_left < 0 && root->left) 
    { // LR
        // Left Rotate on Left child
        root->left = left_rotate(root->left);
        // Update Left child
        // Right Rotate on cur
        root = right_rotate(root);
    } 
    else if (bf_cur < -1 && bf_right > 0 && root->right) 
    { // RL
        // Right Rotate on right child
        root->right = right_rotate(root->right);
        // Update Right child
        // Left Rotate on cur
        root = left_rotate(root);
    }

    return root;
}