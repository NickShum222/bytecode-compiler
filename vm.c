#include "vm.h"
#include "common.h"
#include "compiler.h"
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

InterpretResult interpret(const char *source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }
  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;

  InterpretResult result = run();

  freeChunk(&chunk);
  return result;
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)                                    // reads the byte pointed by vm.ip, then increments vm.ip (post increment)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) // Reads the next byte in the bytecode which pointer to where the constant is in the constant pool, then increments the ip pointer by a byte

  // define a macro to avoid repeating code 4 times
  // The do .. while(false) gives a way to contain multiple statements inside a block that also permits a semicolon at the end
#define BINARY_OP(op) \
  do {                \
    double b = pop(); \
    double a = pop(); \
    push(a op b);     \
  } while (false)

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
    case OP_ADD:
      BINARY_OP(+);
      break;
    case OP_SUBTRACT:
      BINARY_OP(-);
      break;
    case OP_MULTIPLY:
      BINARY_OP(*);
      break;
    case OP_DIVIDE:
      BINARY_OP(/);
      break;
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
#undef BINARY_OP
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
