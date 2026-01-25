/*
 *
 * */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool hadError;
  bool panicMode; // while panic mode flag is set, suppress ant other errors that get detected, used to avoid cascading error messages after the first syntax error is detected
} Parser;

typedef enum { // Precedence levels in order from lowest to highest
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(); // ParseFn is a type for a pointer to a function that takes not arguments and returns void
/* Ex. void (*f)()
 *
 * f = hello;
 *
 * f(); // calls hello();
 *
 * basically, ParseFn is a variable type that can store a function and call it later
 * */

typedef struct {
  ParseFn prefix; // pointer to a function
  ParseFn infix;
  Precedence precedence;
} ParseRule;

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
static void grouping();
static void unary();
static void parsePrecedence(Precedence precedence);
static void binary();
static ParseRule *getRule(TokenType type);
static void literal();
static void string();

ParseRule rules[] = {
    // Maps each Token type to its prefix, infix and precedence level
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};
/**
 * @brief 
 *
 * @param source 
 * @param chunk 
 * @return 
 */
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
/**
 * @brief consumes the token if it matches the tokentype, else outputs error
 *
 * @param type check if current tokentype matches this type
 * @param message error message
 */
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
#ifdef DEBUG_PRINT_CODE
  if (!parser.hadError) {
    disassembleChunk(currentChunk(), "code");
  }

#endif
}
static void emitReturn() {
  emitByte(OP_RETURN);
}

static void expression() {
  // match each token type to a different kind of expression
  parsePrecedence(PREC_ASSIGNMENT); // Lowest precedence so it can parse any expression
}

static void number() {
  double value = strtod(parser.previous.start, NULL); // assume the number literal has already been consumed and is stored in previous
  emitConstant(NUMBER_VAL(value));
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

/**
 * @brief parses the inner group inside the parentheses
 * @note assume the initial '(' has already been consumed
 */
static void grouping() {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary() {
  TokenType operatorType = parser.previous.type;

  parsePrecedence(PREC_UNARY); // compile the operand (rhs), parse everything at precendence level UNARY or higher

  switch (operatorType) {
  case TOKEN_BANG:
    emitByte(OP_NOT);
    break;
  case TOKEN_MINUS:
    emitByte(OP_NEGATE);
    break;
  default:
    return; // unreachable
  }
}

/**
 * @brief starts at the current token and parses any expression at the given precedenve level or higher
 *
 * @param precedence 
 */
static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix; // get the corresponding expression function for the previous token
  if (prefixRule == NULL) {                                   // if this token cant be the FIRST token of an expression, then it must be a syntax error
    /* 
    Valid Ways to start an expression:
      NUMBER        → 42
      (             → (1 + 2)
      -             → -x
      !             → !flag
      IDENTIFIER    → foo

    Invalid Ways to start an expression:
      +
      *
      ==
      )
      ;
    */
    error("Expect expression.");
    return;
  }
  prefixRule();

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule();
  }
}

static void binary() {
  TokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType);
  parsePrecedence((Precedence)(rule->precedence + 1));

  switch (operatorType) {
  case TOKEN_EQUAL_EQUAL:
    emitByte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emitByte(OP_GREATER);
    break;
  case TOKEN_LESS:
    emitByte(OP_LESS);
    break;
  case TOKEN_BANG_EQUAL:
    emitBytes(OP_EQUAL, OP_NOT);
    break;
  case TOKEN_GREATER_EQUAL:
    emitBytes(OP_LESS, OP_NOT);
    break;
  case TOKEN_LESS_EQUAL:
    emitBytes(OP_GREATER, OP_NOT);
    break;
  case TOKEN_PLUS:
    emitByte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emitByte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emitByte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emitByte(OP_DIVIDE);
    break;
  default:
    return; // unreachable
  }
}
/**
 * @brief look up the corresponding infix, prefix, and precedence level
 *
 * @param type 
 * @return 
 */
static ParseRule *getRule(TokenType type) {
  return &rules[type];
}
static void literal() {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emitByte(OP_FALSE);
    break;
  case TOKEN_NIL:
    emitByte(OP_NIL);
    break;
  case TOKEN_TRUE:
    emitByte(OP_TRUE);
    break;
  default:
    return; //Unreachable
  }
}

static void string() {
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}
