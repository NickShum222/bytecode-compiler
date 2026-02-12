/*
 *
 * */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef void (*ParseFn)(bool canAssign); // ParseFn is a type for a pointer to a function that takes not arguments and returns void
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

typedef struct {
  Token name;
  int depth;
} Local;

typedef struct {
  Local locals[UINT8_COUNT]; // array of locals that are in scope during each point in the compilation process
  int localCount;            // tracks how many locals are in scope
  int scopeDepth;            // number of blocks surrount the current bit of code we're compiling
} Compiler;

Parser parser;
Compiler *current = NULL;
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
static void number(bool canAssign);
static void emitConstant(Value value);
static uint8_t makeConstant(Value value); // add the value to the constant table, then emit OP_CONSTANT instr
static void grouping(bool canAssign);
static void unary(bool canAssign);
static void parsePrecedence(Precedence precedence);
static void binary(bool canAssign);
static ParseRule *getRule(TokenType type);
static void literal(bool canAssign);
static void string(bool canAssign);
static void declaration();
static void statement();
static bool match(TokenType type);
static bool check(TokenType type);
static void printStatement();
static void synchronize();
static void varDeclaration();
static void defineVariable(uint8_t global);
static uint8_t parseVariable(const char *errorMessage);
static uint8_t identifierConstant(Token *name);
static void variable(bool canAssign);
static void namedVariable(Token name, bool canAssign);
static void expressionStatement();

static void initCompiler(Compiler *compiler);
static void block();
static void beginScope();
static void endScope();
static void declareVariable();
static void addLocal(Token name);
static bool identifiersEqual(Token *a, Token *b);
static int resolveLocal(Compiler *compiler, Token *name);
static void markInitialized();

static void ifStatement();
static int emitJump(uint8_t instruction);
static void patchJump(int offset);

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
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
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

  Compiler compiler;
  initCompiler(&compiler);

  compilingChunk = chunk;

  parser.hadError = false;
  parser.panicMode = false;

  advance(); // consumes the next token

  while (!match(TOKEN_EOF)) {
    declaration();
  }

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
    fprintf(stderr, " at end\n");
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

/**
 * @brief writes a chunk to the array
 *
 * @param byte 
 */
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

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL); // assume the number literal has already been consumed and is stored in previous
  emitConstant(NUMBER_VAL(value));
}
static void emitConstant(Value value) {
  emitBytes(OP_CONSTANT, makeConstant(value));
}

static void initCompiler(Compiler *compiler) {
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  current = compiler;
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
static void grouping(bool canAssign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign) {
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
  bool canAssign = precedence <= PREC_ASSIGNMENT; // since PREC_ASSIGNMENT is the lowest-precedence expr, the only time we allow an assisngment is when parsing an assignment expression or top-level expression like in an expression statement
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static void binary(bool canAssign) {
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
static void literal(bool canAssign) {
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

static void string(bool canAssign) {
  emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

/**
 * @brief declaration has the grammar: declaration -> classDecl | funDecl | varDecl | statement
 */
static void declaration() {
  if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }
  if (parser.panicMode)
    synchronize();
}
/**
 * @brief skip tokens indiscriminately until we reach something that looks like a statement boundary
 */
static void synchronize() {
  parser.panicMode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:; // do nothing
    }

    advance();
  }
}

/**
 * @brief statement -> exprStmt | printStmt | block
 */
static void statement() {
  if (match(TOKEN_PRINT)) {
    printStatement();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else {
    expressionStatement();
  }
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void beginScope() {
  current->scopeDepth++;
}

static void endScope() {
  current->scopeDepth--;

  while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) { // end the current scope
    emitByte(OP_POP);
    current->localCount--;
  }
}
/**
 * @brief if the current token has the given type, consume it and return true
 *
 * @param type 
 * @return 
 */
static bool match(TokenType type) {
  if (!check(type))
    return false;

  advance();
  return true;
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static void printStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_PRINT);
}

static void expressionStatement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(OP_POP);
}

static void varDeclaration() {
  uint8_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(global);
}

/**
 * @brief outputs the bytecode instruction that defines the new variable and stores its initial value
 *
 * @param global 
 */
static void defineVariable(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized(); // once the variable's initializer has been compiler, mark it initialized
    return;
  }
  emitBytes(OP_DEFINE_GLOBAL, global);
}

/**
 * @brief 
 */
static void markInitialized() {
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

/**
 * @brief 
 *
 * @param errorMessage 
 * @return idx of the variable constant in the constant pool
 */
static uint8_t parseVariable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);

  declareVariable();
  if (current->scopeDepth > 0) // exit the function if we're in a local scope
    return 0;
  return identifierConstant(&parser.previous);
}

/**
 * @brief records the existence of the variable, only do this for locals, adds to teh compiler's list of variables in the current scope
 */
static void declareVariable() {
  if (current->scopeDepth == 0)
    return;

  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) { // we disallow two variables with the same name in the same local scope
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error("Already a variable with this name in this scope.");
    }
  }

  addLocal(*name);
}

static bool identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length)
    return false;

  return memcmp(a->start, b->start, a->length) == 0;
}
/**
 * @brief initializes the next available local in teh compiler's array of variables
 *
 * @param name 
 */
static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }
  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1; // -1 = "uninitialized state"
}

/**
 * @brief takes the given token and adds its lexeme to the chunks constant table as a string
 *
 * @param name 
 * @return index of that constant in the constant table
 */
static uint8_t identifierConstant(Token *name) {
  return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static void variable(bool canAssign) {
  namedVariable(parser.previous, canAssign);
}

/**
 * @brief lets the VM know where the variable exists in the constant table
 *
 * @param name 
 * @param canAssign 
 */
static void namedVariable(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);

  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else {
    arg = identifierConstant(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }

  if (canAssign && match(TOKEN_EQUAL)) { // if we see an EQUAL token, we compile it as an assignment or setter instead of a variable access or getter
    expression();
    emitBytes(setOp, (uint8_t)arg);
  } else {
    emitBytes(getOp, (uint8_t)arg);
  }
}

/**
 * @brief find the location of the identifer we are looking for that is most in scope
 *
 * @param compiler 
 * @param name 
 * @return -1 to signal that we havent found it and assume its a global variable
 */
static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) { // walk backwards so we find the last declared variable. this ensures inner vars shadow locals with the same name
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer. ");
      }
      return i;
    }
  }
  return -1;
}

static void ifStatement() {
  consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'. ");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition. ");

  int thenJump = emitJump(OP_JUMP_IF_FALSE);
  statement();

  patchJump(thenJump);
}

/* BACKPATCHING: 
 * First emit the jump instruction with a placeholder offset as we dont know how many bytes for the ip to jump to
 * Once we compile the body of the condition statement, we replace the placeholder offset with the actual offset
 * */

/**
 * @brief emit the jump with a placeholder value
 *
 * @param instruction 
 * @return the offset of the emitted instruction in the chunk
 */
static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff); // use two bytes for the jump offset
  emitByte(0xff); // 16 bit offset means we can jump through 65536 bytes of code
  return currentChunk()->count - 2;
}

/**
 * @brief goes back into the bytecode and replaces the operand at the given location with the calculated jump offset
 *
 * @param offset the location of the OP_JUMP_IF_FALSE opcode
 */
static void patchJump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself
  int jump = currentChunk()->count - offset - 2; // current location after compiling the body - location of OP_JUMP_IF_FALSE - 2 = the size of the compiled body

  if (jump > UINT16_MAX) {
    error("Too much code to jump over.");
  }
  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}
