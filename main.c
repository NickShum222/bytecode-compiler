#include "chunk.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
  Chunk chunk;
  initChunk(&chunk);
  int constant = addConstant(&chunk, 1.2); // return the offset of the constant
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123); // write the offset to the chunk so it knows where the 1.2 is located

  writeChunk(&chunk, OP_RETURN, 123);

  disassembleChunk(&chunk, "test chunk");
  freeChunk(&chunk);
  return 0;
}
