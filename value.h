// store our constants shared in a "pool"
#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value; // used to abstract, can change the type itself here without redefining value anywhere else

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);
void freeValueArray(ValueArray *array);
void printValue(Value value);

#endif
