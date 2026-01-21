#include "vm.h"
#include "common.h"
#include <stdio.h>

VM vm;

static InterpretResult run();
void initVM() {
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
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      printValue(constant);
      printf("\n");
      break;
    }
    case OP_RETURN: {
      return INTERPRET_OK; // exit the loop
    }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
}
