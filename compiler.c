#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode; // while panic mode flag is set, suppress ant other errors that get detected, used to avoid cascading error messages after the first syntax error is detected
} Parser;

Parser parser;
Chunk *compilingChunk;

static void advance(); // steps forward through the token stream, stores the previouss and current token
static void expression();
static void errorAtCurrent(const char *meesage);
static void error(const char *messsage);
static void errorAt(Token *token, const char *message);
static void consume(TokenType type, const char *message); // it resads the next token but also validates that the token has an expected type
static void emitByte(uint8_t byte);                       // append a ssingle byte to the chunk, byute can be opcode or operand
static void emitBytes(uint8_t byte1, uint8_t byte2);
static Chunk *currentChunk();
static void endCompiler();
static void emitReturn();
static void number();
static void emitConstant(Value value);
static uint8_t makeConstant(Value value); // add the value to the constant table, then emit OP_CONSTANT instr

bool compile(const char *source, Chunk *chunk) {
  initScanner(source);
  compilingChunk = chunk;

  parser.hadError = false;
  parser.panicMode = false;

  advance(); // consumes the next token
  expression();

  consume(TOKEN_EOF, "Expect end of expression.");
  endCompiler();

  return !parser.hadError;
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR)
      break;

    errorAtCurrent(parser.current.start);
  }
}
static void errorAtCurrent(const char *message) {
  errorAt(&parser.current, message);
}

static void error(const char *message) {
  errorAt(&parser.previous, message);
}

static void errorAt(Token *token, const char *message) {
  if (parser.panicMode)
    return;
  parser.panicMode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing
  } else {
    fprintf(stderr, " at '%s.*s'", token->length, token->start);
    parser.hadError = true;
  }
}
static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static Chunk *currentChunk() {
  return compilingChunk;
}
static void endCompiler() {
  emitReturn();
}
static void emitReturn() {
  emitByte(OP_RETURN);
}

static void expression() {
  // match eac token type to a different kind of expression
}

static void number() {
  double value = strtod(parser.previous.start, NULL); // assume the number literal has already been consumed and is stored in previous
  emitConstant(value);
}
static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}
/**
 * @brief inserts the value to value constant pool, ensures we dont have too many constants
 *
 * @param value 
 * @return 
 */
static uint8_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value);

  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}
