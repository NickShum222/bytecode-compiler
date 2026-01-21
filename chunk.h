// Each "chunk" refere to sequences of bytecode
#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

typedef enum {
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
} Chunk; // Dynamic array of chunks

void initChunk(Chunk *chunk); // Constructor
void freeChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte); // append a byte to end of chunk
#endif
