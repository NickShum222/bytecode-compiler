#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
  (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
  Obj *object = (Obj *)reallocate(NULL, 0, size);
  object->type = type;

  object->next = vm.objects;
  vm.objects = object; // set the head of the pointer to the newly allocated object
  return object;
}

/**
 * @brief allocates an ObjString using ALLOCATE_OBJ (initiailzes Obj as well)
 *
 * @param chars 
 * @param length 
 * @return 
 */
ObjString *allocateString(char *chars, int length, uint32_t hash) {
  ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  tableSet(&vm.strings, string, NIL_VAL); // treated as a hash set, we dont really care about the values themselves
  return string;
}

/**
 * @brief copy string by first allocating space using ALLOCATE, then copies it over followed by null terminating char
 *
 * @param chars 
 * @param length 
 * @return 
 */
ObjString *copyString(const char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash); // look up in the string table first

  if (interned != NULL) // if we find it, instead of "copying", we just return a reference to taht string
    return interned;

  // otherwise, allocate a new string and store it in the table
  char *heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(heapChars, length, hash);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}

uint32_t hashString(const char *key, int length) {
  int32_t hash = 216613261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }

  return hash;
}

ObjString *takeString(char *chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm.strings, chars, length, hash); // find the string in the hash set/table

  // if we find it, before we return it, we free the memory for the string that was passed in
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return allocateString(chars, length, hash);
}
