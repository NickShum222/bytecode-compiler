// Each "chunk" refere to sequences of bytecode
#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT, // takes a single byte operand that specifies which constant to load from the chunk's constant array
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
int addConstant(Chunk *chunk, Value value);            // add a new constant to the chunk
// TODO: getLine(Chunk* chunk) -> returns the line number of the chunk
#endif
