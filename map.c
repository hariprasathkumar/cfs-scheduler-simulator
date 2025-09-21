#include <stddef.h>
#include <stdlib.h>
#include "avl.h"
#include "map.h"
#include <stdio.h>

#define LOAD_FACTOR_THRESHOLD    (0.7f)

int hash_key(struct hash *hash, long long key)
{
    return key % hash->table_size;
}

static int hash2(struct hash *map, long long key)
{
    return 1 + (map->hash_fn(map, key) % (map->table_size - 1));
}

static inline void update_load_factor(struct hash *map)
{
    if (map)
    map->load_factor = (float)map->num_of_elements / map->table_size;
}

static char isPrime(long long i)
{
    if (i <= 1) return 0;
    if (i == 2) return 1;
    if (i % 2 == 0) return 0;

    for (size_t j = 3; j * j <= i; j++)
    {
        if (i % j == 0) return 0;
    }

    return 1; // no number below its sqrt is divisible
}

static long long getPrime(long long start)
{
    while (!isPrime(start)) start++;
    return start;
}

int map_init(struct hash **hash, long long no_of_slots,
    int (*hash_fn)(struct hash *hash, long long key)) 
{    
    long long taken_table_size = getPrime(no_of_slots);
    #ifdef DEBUG
    fprintf(stdout, "Taken table size is %lld\n", taken_table_size);
    #endif

    *hash = (struct hash *) calloc(1, sizeof(struct hash));
    if (!*hash)
    {
        #ifdef DEBUG
        fprintf(stderr, "hash alloc failed\n");
        #endif
        return -1;
    }

    (*hash)->hashmap = (struct key_value_pair **) calloc(taken_table_size, sizeof(struct key_value_pair *));
    if (!(*hash)->hashmap) 
    {
        #ifdef DEBUG
        fprintf(stderr, "hash alloc failed!");
        #endif
        return -1;
    }

    (*hash)->num_of_elements = 0;
    (*hash)->table_size = taken_table_size;
    (*hash)->load_factor = 0.0f;
    (*hash)->hash_fn = hash_fn;
    if (!hash_fn)
    {
        (*hash)->hash_fn = hash_key;
        #ifdef DEBUG
        fprintf(stderr, "hash function null, defaulting to internal\n");
        #endif
    }
    return 0;
}

/*
    Double hashing uses a hash for start index another has for step increment
    important thigs is the step is a coprime with table size 
    this is done by taking the table size - 1 modulo
    coprime ensures that all slots are vistied on a linear probe

    index = (hash1(key) + i*hash2(key)) %  table_size
    hash1(key) = key % table_size
    hash2(key) = 1 + (hash1(key) % N-1) => modulo N-1 ensure 0..N-2 range +1 is to avoid infinite loop / mov ethe step so 1..N-1 (no 0, causes loop) 
*/

static struct hash *rehash(struct hash *map)
{
    long long cur_size = map->table_size;
    long long new_size = getPrime(cur_size * 2);

    struct hash *newmap = (struct hash *)calloc(1, sizeof(struct hash));
    if (!newmap) 
    {
        #ifdef DEBUG
        fprintf(stderr, "rehash alloc failed!\n");
        #endif
        return NULL;
    }

    newmap->hashmap = (struct key_value_pair **)calloc(new_size, sizeof(struct key_value_pair *));
    if (!newmap->hashmap) 
    {
        #ifdef DEBUG
        fprintf(stderr, "rehash alloc failed!\n");
        #endif
        free_wrapper(newmap, "rehash");
        return NULL;
    }
    newmap->table_size = new_size;
    newmap->hash_fn = map->hash_fn;

    for (size_t i = 0; i < cur_size; i++) 
    {
        struct key_value_pair *entry = map->hashmap[i];
        if (entry && entry != TOMBSTONE) 
        {
            long long key = entry->key;
            // probe into new table until empty slot
            for (long long j = 0; j < new_size; j++) 
            {
                long long index = ((long long)newmap->hash_fn(newmap, key) + j * hash2(newmap, key)) % new_size;
                if (!newmap->hashmap[index]) 
                {
                    newmap->hashmap[index] = entry; // move pointer directly
                    newmap->num_of_elements++;
                    break;
                }
            }
        }
    }

    // free only the old table array, not the key_value_pair structs
    free_wrapper(map->hashmap, "rehashed");
    free_wrapper(map, "rehashed");

    return newmap;
}

struct task *map_lookup(struct hash **hash, long long key)
{
    if (!hash || !*hash) 
    {
        #ifdef DEBUG
        fprintf(stderr, "invalid pointer\n");
        #endif
        return NULL;
    }

    struct hash *map = *hash;
    for (long long i = 0; i < map->table_size; i++) 
    {
        long long index = ((long long)map->hash_fn(map, key) + i * hash2(map, key)) % map->table_size;

        if (!map->hashmap[index]) return NULL;
        else if (map->hashmap[index] == TOMBSTONE) continue;
        else if (map->hashmap[index]->key == key)
        {
            return map->hashmap[index]->val;
        }
        else
        {
            // continue probe
        }
    }

    return NULL;
}
void free_wrapper(void * p, const char *owner)
{
    #ifdef DEBUG
    fprintf(stdout, "%s %p\n", owner, p);
    #endif
    free(p);
}

void free_map(struct hash *map)
{
    for (size_t i = 0; i < map->table_size; i++) 
    {
        if (map->hashmap[i] && map->hashmap[i] != TOMBSTONE) 
        {
            free_wrapper(map->hashmap[i], "free_map: i");
        }
    }
    if (map)
    {
        free_wrapper(map->hashmap, "free_map: table");
        free_wrapper(map, "free_map: p");
    }
}

void map_insert(struct hash **hash, long long key, struct task *val)
{
    if (!hash || !*hash) 
    {
        #ifdef DEBUG
        fprintf(stderr, "invalid pointer\n");
        #endif
        return;
    }

    struct hash *map = *hash;
    for (long long i = 0; i < map->table_size; i++) 
    {
        long long index = ((long long)map->hash_fn(map, key) + i * hash2(map, key)) % map->table_size;

        if (!map->hashmap[index] || map->hashmap[index] == TOMBSTONE)
        {
            struct key_value_pair *n = (struct key_value_pair *) malloc(sizeof(struct key_value_pair));
            n->val = val;
            n->key = key;
            map->hashmap[index] = n;
            map->num_of_elements++;
            
            update_load_factor(map);
            if (map->load_factor > LOAD_FACTOR_THRESHOLD)
            {
                struct hash *new_hash = rehash(map);
                if (new_hash)
                {
                    *hash = new_hash;
                }
            }
            break;
        }
        else if (map->hashmap[index] && map->hashmap[index]->key == key) // overwrite
        {
            map->hashmap[index]->val = val;
            break;
        }
    }
}

void map_delete(struct hash **hash, long long key)
{
    if (!hash || !*hash) 
    {
        #ifdef DEBUG
        fprintf(stderr, "invalid pointer\n");
        #endif
        return;
    }

    struct hash *map = *hash;
    for (long long i = 0; i < map->table_size; i++) 
    {
        long long index = ((long long)map->hash_fn(map, key) + i * hash2(map, key)) % map->table_size;

        if (!map->hashmap[index]) return; // key not found
        if (map->hashmap[index] == TOMBSTONE) continue;
        if (map->hashmap[index]->key == key) 
        {
            free_wrapper(map->hashmap[index], "Map delete");
            map->hashmap[index] = TOMBSTONE;
            map->num_of_elements--;
            update_load_factor(map);
            return;
        }
    }
}

void map_print_all(struct hash *hash)
{
    for (long long i = 0; i < hash->table_size; i++)
    {
        if (hash->hashmap[i] && hash->hashmap[i] != TOMBSTONE)
        {
            fprintf(stdout, "Map contents: key %lld value %p\n", hash->hashmap[i]->key, hash->hashmap[i]->val);
        }
    }
}