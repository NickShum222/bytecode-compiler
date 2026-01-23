// store our constants shared in a "pool"
#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER
} ValueType;

//typedef double Value; // used to abstract, can change the type itself here without redefining value anywhere else

typedef struct {
  ValueType type; // 4 bytes as enum is typically stored as an int (4 bytes)
  union {         // union lets diff variables share the same memory location, at any given time, only one of the union members is meant to be used
    bool boolean;
    double number;
  } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

// Given a Value of the right type, they unwrap it and return the corresponding raw C value
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

// Each of these takes a C value of the appropriate type and produces a value that has the correct type tag and contains the value
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
// Value value = BOOL_VAL(true);
// double number = AS_NUMBER(value);

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value);

#endif
