#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

VM vm;

static InterpretResult run();
static void resetStack();
static Value peek(int distance);
static void runtimeError(const char *format, ...); // varying number of arugments
static bool isFalsey(Value value);
static void concatenate();

void initVM() {
  resetStack();
  vm.objects = NULL;
  initTable(&vm.globals);
  initTable(&vm.strings);
}
void freeVM() {
  freeTable(&vm.strings);
  freeTable(&vm.globals);
  freeObjects();
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
#define READ_STRING() AS_STRING(READ_CONSTANT());

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
    case OP_EQUAL: {
      Value b = pop();
      Value a = pop();
      push(BOOL_VAL(valuesEqual(a, b)));
      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm.globals, name, peek(0));
      pop();
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm.globals, name, &value)) {
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value);
      break;
    }

    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      if (tableSet(&vm.globals, name, peek(0))) { // if the variable hasnt been defined yet, it's a runtime error to try to assign to it
        tableDelete(&vm.globals, name);
        runtimeError("Undefined variable '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_GET_LOCAL: { // loads teh value from the index and pushes it on top of the stack where later instructions can find it
      uint8_t slot = READ_BYTE();
      push(vm.stack[slot]);
      break;
    }
    case OP_SET_LOCAL: { // takes the assigned value from the top of the stack and stores it in the stack slot corresponding to the local variable
      uint8_t slot = READ_BYTE();
      vm.stack[slot] = peek(0);
      break;
    }

    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_ADD: {
      if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
        concatenate();
      } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
        double b = AS_NUMBER(pop());
        double a = AS_NUMBER(pop());
        push(NUMBER_VAL(a + b));
      } else {
        runtimeError("Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }

      break;
    }
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
    case OP_PRINT: {
      printValue(pop());
      printf("\n");
      break;
    }
    case OP_POP:
      pop();
      break;
    case OP_RETURN: {
      // exit the interpreter
      return INTERPRET_OK; // exit the loop
    }
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_STRING
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

static void concatenate() {
  ObjString *b = AS_STRING(pop());
  ObjString *a = AS_STRING(pop());

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);

  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';
  ObjString *result = takeString(chars, length);
  push(OBJ_VAL(result));
}
