
#ifndef _AVL_H
#define _AVL_H
struct task 
{
    long long vmruntime;
    long long pid;
    long long height;
    struct task *left;
    struct task *right;
};

struct task *find_by_pid(struct task *root, long long pid, long long vmruntime);
void print_tree(struct task *root);
struct task *find_min(struct task *root);
struct task *insert(struct task *root, struct task *node);
struct task *delete(struct task *root, struct task **bubbled_node, long long pid, long long vmruntime);

#endif