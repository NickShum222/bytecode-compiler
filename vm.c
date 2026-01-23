#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

VM vm;

static InterpretResult run();
static void resetStack();
static Value peek(int distance);
static void runtimeError(const char *format, ...); // varying number of arugments
static bool isFalsey(Value value);

void initVM() {
  resetStack();
}
void freeVM() {
}

/**
 * @brief Initializes chunk, compiles, and executes the program
 *
 * @param source 
 * @return 
 */
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

/**
 * @brief executes the source file using the instruction pointer
 *
 * @return 
 */
static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)                                    // reads the byte pointed by vm.ip, then increments vm.ip (post increment)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]) // Reads the next byte in the bytecode which pointer to where the constant is in the constant pool, then increments the ip pointer by a byte

  // define a macro to avoid repeating code 4 times
  // The do .. while(false) gives a way to contain multiple statements inside a block that also permits a semicolon at the end
#define BINARY_OP(valueType, op)                      \
  do {                                                \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      runtimeError("Operands must be numbers.");      \
      return INTERPRET_RUNTIME_ERROR;                 \
    }                                                 \
    double b = AS_NUMBER(pop());                      \
    double a = AS_NUMBER(pop());                      \
    push(valueType(a op b));                          \
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
    case OP_FALSE:
      push(BOOL_VAL(false));
      break;
    case OP_NIL:
      push(NIL_VAL);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true));
      break;
    case OP_ADD:
      BINARY_OP(NUMBER_VAL, +);
      break;
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;
    case OP_NOT:
      push(BOOL_VAL(isFalsey(pop())));
      break;
    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0))) {
        runtimeError("Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop())));
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

/**
 * @brief Returns a value from stack but doesnt pop
 *
 * @param distance how far down from the top of the stack to look, 0 = top, 1 = 1 slot down
 * @return 
 */
static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static void runtimeError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args); // like printf but takes in va_list
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = vm.chunk->lines[instruction];

  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}
