#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#define IS_STRING(value) isObjType(value, OBJ_STRING)

#define AS_STRING(value) ((ObjString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJ(value))->chars)

typedef enum {
  OBJ_STRING,
} ObjType;

struct Obj {
  ObjType type;
  struct Obj *next;
};

struct ObjString {
  Obj obj;
  int length;
  char *chars;
  uint32_t hash; // hash code for its string
};

ObjString *copyString(const char *chars, int length);

static inline bool isObjType(Value value, ObjType type) { // inline tells the compiler to treat is as one line to avoid function overhead
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

void printObject(Value value);

ObjString *allocateString(char *chars, int length, uint32_t hash);
uint32_t hashString(const char *key, int length);

ObjString *takeString(char *chars, int length);

#endif
