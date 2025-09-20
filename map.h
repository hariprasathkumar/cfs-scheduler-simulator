
#ifndef _MAP_H
#define _MAP_H

struct hash 
{
    struct key_value_pair **hashmap;
    long long num_of_elements;
    long long table_size;
    float load_factor;
    int (*hash_fn)(struct hash *hash, long long key);
};
struct key_value_pair 
{
    long long key;
    struct task *val;
};

void map_init(struct hash **hash, long long no_of_slots,
    int (*hash_fn)(struct hash *hash, long long key));

struct task *map_lookup(struct hash **hash, long long key);
void map_insert(struct hash **hash, long long key, struct task *val);
void map_delete(struct hash **hash, long long key);
struct task *map_lookup(struct hash **hash, long long key);
void free_map(struct hash *map);

#endif