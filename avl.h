


struct task *find_by_pid(struct task *root, long long pid, long long vmruntime);
void print_tree(struct task *root);
struct task *find_min(struct task *root);
struct task *insert(struct task *root, long long pid, long long vmruntime);
struct task *delete(struct task *root, long long pid, long long vmruntime);