#include <stdlib.h>
#include <string.h>


typedef struct item {
    long key;
    long value;
    struct item *next;
} HashItem;

typedef struct {
    HashItem **entries;
    int size;
} HashMap;

#define HASH_MAP_INIT_SIZE 1024
#define HASH_ITEM_NOT_FOUND -1
/*
 * Core functions
 *
 */

HashMap *createHashMap();
void insertHashMap(HashMap *hashMap, long key, long value);
long searchHashMap(HashMap *hashMap, long key);
void deleteHashMap(HashMap *hashMap, long key);
void freeHashMap(HashMap *hashMap);

HashItem * pop(HashMap *hashMap, long key);

int getHash(long key, int size);
