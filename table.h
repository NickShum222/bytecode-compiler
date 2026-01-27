#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

void initTable(Table *table);
void freeTable(Table *table);

/**
 * @brief fetch value from a table given a key
 *
 * @param table 
 * @param key 
 * @param value output parameter
 * @return true if it finds an entry with key, false otherwise
 */
bool tableGet(Table *table, ObjString *key, Value *value);

/**
 * @brief deletes an entry given a key by adding a sentinel entry called a tombstone
 *
 * @param table 
 * @param key 
 * @return 
 */
bool tableDelete(Table *table, ObjString *key);

/**
 * @brief adds the given key/value pair to the given hash table. if an entry for that key is present, the new value overwrites the old value. 
 *
 * @param table 
 * @param key 
 * @param value 
 * @return True if a new entry was added
 */
bool tableSet(Table *table, ObjString *key, Value value);

/**
 * @brief copy all entries from one hash table to another
 *
 * @param from 
 * @param to 
 */
void tableAddAll(Table *from, Table *to);

/**
 * @brief look for a string in the table
 *
 * @param table 
 * @param chars 
 * @param length 
 * @param hash 
 * @return 
 */
ObjString *tableFindString(Table *table, const char *chars, int length, uint32_t hash);

#endif
