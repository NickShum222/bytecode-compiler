#include "chunk.h"
#include "memory.h"
#include <stdlib.h>

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  initValueArray(&chunk->constants);
}

void freeChunk(Chunk *chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity); // Deallocate all of the memory
  freeValueArray(&chunk->constants);
  initChunk(chunk); // Zero out the fields
}

void writeChunk(Chunk *chunk, uint8_t byte) {
  if (chunk->capacity < chunk->count + 1) { // if at capacity, we need to grow the array
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count++] = byte;
}

int addConstant(Chunk *chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1; // used to locate that same constant later
}
