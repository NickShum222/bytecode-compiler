#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256 // TODO: dynamically increase the size of the stack

typedef struct {
  Chunk *chunk;
  uint8_t *ip; // Instruction pointer
  // We use a pointer instaead of an integer index bc its faster to dereference a pointer than look up an element in an array by index

  Value stack[STACK_MAX]; // fixed size for now
  Value *stackTop;        // stackTop points just past the element containing the top value, can tell if stack is empty if its pointing at element 0 in the array
  Obj *objects;           // poitner to the head of the list

  Table strings;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult; // when we have a compiler that reports static errors and VM that detects runtime errors, the interpreter will use this to know how to set the exit code

void initVM();
void freeVM();
InterpretResult interpret(const char *source);
void push(Value value);
Value pop();

extern VM vm; // expose the vm object

#endif
