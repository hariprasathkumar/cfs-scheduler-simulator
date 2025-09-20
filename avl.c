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

struct task *avl_find_by_pid(struct task *root, long long pid, long long vmruntime)
{
    if (!root) return NULL;
    int cmp = compare(vmruntime, pid, root->vmruntime, root->pid);
    if (cmp == 0) return root;
    else if (cmp < 0) return avl_find_by_pid(root->left, pid, vmruntime);
    else return avl_find_by_pid(root->right, pid, vmruntime);
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

static struct task *right_rotate(struct task *node) 
{
    struct task *root = node->left;
    struct task *subtree = root->right;

    if (root)
        root->right = node;

    node->left = subtree;

    node->height = 1 + max(get_height(node->left), get_height(node->right));
    root->height = 1 + max(get_height(root->left), get_height(root->right));

    return root;
}

static struct task *left_rotate(struct task *node) 
{
    struct task *root = node->right;
    struct task *subtree = root->left;

    if (root) 
        root->left = node;

    node->right = subtree;

    node->height = 1 + max(get_height(node->left), get_height(node->right));
    root->height = 1 + max(get_height(root->left), get_height(root->right));

    return root;
}

void avl_print_tree(struct task *root)
{
    if (!root) return;

    avl_print_tree(root->left);
    printf("%lld %lld\n", root->pid, root->vmruntime);
    avl_print_tree(root->right);
}

struct task *avl_find_min(struct task *root) 
{
    if (!root) return NULL;
    if (!root->left) return root;

    return avl_find_min(root->left);
}

struct task *avl_insert(struct task *root, struct task *node) 
{
    if (!root) 
    {  
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