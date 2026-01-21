#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"

typedef struct {
  Chunk *chunk;
  uint8_t *ip; // Instruction pointer
  // We use a pointer instaead of an integer index bc its faster to dereference a pointer than look up an element in an array by index
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult; // when we have a compiler that reports static errors and VM that detects runtime errors, the interpreter will use this to know how to set the exit code

void initVM();
void freeVM();

InterpretResult interpret(Chunk *chunk);

#endif
