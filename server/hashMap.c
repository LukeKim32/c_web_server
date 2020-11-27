#include "hashMap.h"


HashMap *createHashMap() {

  HashMap *hashMap = (HashMap *)malloc(sizeof(HashMap));
  hashMap->entries = malloc(
      sizeof(HashItem *) * HASH_MAP_INIT_SIZE
  );

  for(int i = 0; i < HASH_MAP_INIT_SIZE; i++) {
    hashMap->entries[i] = NULL;
  }
  hashMap->size = HASH_MAP_INIT_SIZE;
  
  return hashMap;
}

void insertHashMap(HashMap *hashMap,  long key,  long value) {
  
  HashItem * newEntry = malloc(sizeof(HashItem));
  newEntry->key = key;
  newEntry->value = value;
  newEntry->next = NULL;

  int hashIndex = getHash(key, hashMap->size);

  if(hashMap->entries[hashIndex] == NULL) {

    hashMap->entries[hashIndex] = newEntry;

  } else {

    // Search for tail of current hashIndex
    HashItem * current = hashMap->entries[hashIndex];
    while(current->next != NULL) {
      current = current->next;
    }

    current->next = newEntry;
  }

}

long searchHashMap(HashMap *hashMap, long key) {

  int hashIndex = getHash(key, hashMap->size);

  HashItem *current = hashMap->entries[hashIndex];

  while(current != NULL) {

    if(current->key == key) {
      return current->value;
    }

    current = current->next;
  }

  return HASH_ITEM_NOT_FOUND;
}

void deleteHashMap(HashMap *hashMap,  long key) {

  int hashIndex = getHash(key, hashMap->size);

  HashItem * current = hashMap->entries[hashIndex];
  HashItem * previous = current;

  while(current != NULL) {

    if(current->key == key) {
      break;
    }

    previous = current;
    current = current->next;
  }

  if(current == NULL) {
    // Entry not found!

  } else {

    if(current == previous) {
      hashMap->entries[hashIndex] = NULL;

    } else {
      previous->next = current->next;
    }

    free(current);
  }
}


HashItem * pop(HashMap *hashMap,  long key) {

  int hashIndex = getHash(key, hashMap->size);

  HashItem * current = hashMap->entries[hashIndex];
  HashItem * previous = current;

  while(current != NULL) {

    if(current->key == key) {
      break;
    }

    previous = current;
    current = current->next;
  }

  HashItem * target = NULL;

  if(current != NULL) {

    if(current == previous) {
      hashMap->entries[hashIndex] = NULL;

    } else {
      previous->next = current->next;
    }

    target = current;
  }

  return target;
}


void freeHashMap(HashMap *hashMap) {

  HashItem *current_entry;
  HashItem *next_entry;

  for(int i = 0; i < hashMap->size; i++) {

    current_entry = hashMap->entries[i];
    next_entry = current_entry;

    while(next_entry != NULL) {

      next_entry = current_entry->next;

      free(current_entry);

      current_entry = next_entry;
    }
  }

  free(hashMap->entries);
  free(hashMap);
}

int getHash( long key, int size) {
  return (key % size);
}
