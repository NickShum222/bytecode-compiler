#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75 // our load factor

static Entry *findEntry(Entry *entries, int capacity, ObjString *key);
static void adjustCapacity(Table *table, int capacity);

void initTable(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

bool tableSet(Table *table, ObjString *key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);

    adjustCapacity(table, capacity);
  }
  Entry *entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->key == NULL;

  if (isNewKey && IS_NIL(entry->value)) // if the entry we are writing over is not a tombstone, add a count
    table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableGet(Table *table, ObjString *key, Value *value) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);

  if (entry->key == NULL)
    return false;

  *value = entry->value;
  return true;
}

bool tableDelete(Table *table, ObjString *key) {
  if (table->count == 0)
    return false;

  Entry *entry = findEntry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  // place a tombstone in the entry
  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

void tableAddAll(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString *tableFindString(Table *table, const char *chars, int length, uint32_t hash) {
  if (table->count == 0)
    return NULL;

  uint32_t index = hash % table->capacity;

  for (;;) {
    Entry *entry = &table->entries[index];

    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry
      if (IS_NIL(entry->value))
        return NULL;

    } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
      return entry->key;
    }

    index = (index + 1) % table->capacity;
  }
}

/**
 * @brief Finds the entry given a key value, if we have passed a tombstone, we return its bucket
 *
 * @param entries 
 * @param capacity 
 * @param key 
 * @return 
 */
static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
  uint32_t index = key->hash % capacity; // starting point to start searching
  Entry *tombstone = NULL;
  for (;;) {
    Entry *entry = &entries[index];
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        // empty value
        return tombstone != NULL ? tombstone : entry;
      } else {
        // found a tombstone
        if (tombstone == NULL)
          tombstone = entry;
      }
    } else if (entry->key == key) {
      // found the entry
      return entry;
    }
    index = (index + 1) % capacity; // iterate through using linear probing
  }
}

/**
 * @brief Increase the size of the hash table 
 *
 * @param table 
 * @param capacity 
 */
static void adjustCapacity(Table *table, int capacity) {
  Entry *entries = ALLOCATE(Entry, capacity);

  table->count = 0; // dont copy the tombstones over, so start counting again

  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];

    if (entry->key == NULL) {
      continue;
    }

    Entry *dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;

    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);

  table->entries = entries;
  table->capacity = capacity;
}
