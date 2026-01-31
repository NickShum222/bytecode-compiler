// Each "chunk" refere to sequences of bytecode
#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT, // takes a single byte operand that specifies which constant to load from the chunk's constant array
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_NEGATE,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_PRINT,
  OP_POP,
  OP_DEFINE_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t *code; // array of instructions which contain op code and others
  int *lines;    // line number
  ValueArray constants;
} Chunk;

void initChunk(Chunk *chunk); // Constructor
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line); // append a byte to end of chunk

/*
 * @brief Adds a value into chunk through the value constant pool
 *
 * @param chunk pointer to chunk we add value to
 * @param value actual value to be added
 * @return index of where the constant is stored in the chunk
 * */
int addConstant(Chunk *chunk, Value value); // add a new constant to the chunk, returns
// TODO: getLine(Chunk* chunk) -> returns the line number of the chunk
#endif
