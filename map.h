
#ifndef _MAP_H
#define _MAP_H


#define TOMBSTONE                ((void *)-1)
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


int map_init(struct hash **hash, long long no_of_slots,
    int (*hash_fn)(struct hash *hash, long long key)) ;
struct task *map_lookup(struct hash **hash, long long key);
void map_insert(struct hash **hash, long long key, struct task *val);
void map_delete(struct hash **hash, long long key);
struct task *map_lookup(struct hash **hash, long long key);
void free_map(struct hash *map);
void free_wrapper(void * p, const char *owner);
void map_print_all(struct hash *hash);

#endif