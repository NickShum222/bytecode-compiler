#include "vm.h"
#include "common.h"
#include "debug.h"
#include <stdio.h>

VM vm;

static InterpretResult run();
static void resetStack();

void initVM() {
  resetStack();
}
void freeVM() {
}

InterpretResult interpret(Chunk *chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code; // IP is set to the first byte in code

  return run(); // internal helper function
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)                                    // reads the byte pointed by vm.ip, then increments vm.ip (post increment)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) // Reads the next byte in the bytecode which pointer to where the constant is in the constant pool, then increments the ip pointer by a byte

  for (;;) { // while (true) read and execute a single bytecode instruction
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code)); // disassembleInstruction takes in an offset, so we subtract the current state of IP to the first byte of the code array to find it
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant);
      break;
    }
    case OP_NEGATE: {
      push(-pop());
      break;
    }
    case OP_RETURN: {
      printValue(pop());
      printf("\n");
      return INTERPRET_OK; // exit the loop
    }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
}

static void resetStack() {
  vm.stackTop = vm.stack; // same as &stack[0]
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++; // moves the pointer forward by sizeof(Value) bytes
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}
