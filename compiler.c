#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "common.h"
#include "compiler.h"
//#include "error.h"
//#include "file.h"
#include "memory.h"
#include "number.h"
#include "parser.h"
//#include "scanner.h"
#include "object.h"
#include "vm.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

/*
  Copy/pasting the grammar of the clox language (on which FunC is based)
  in the hope that I will one day be able to understand its use.

  program        ? declaration* EOF ;


  declaration    ? classDecl
                 | funDecl
                 | varDecl
                 | statement ;

  classDecl      ? "class" IDENTIFIER ( "<" IDENTIFIER )?
                   "{" function* "}" ;
  funDecl        ? "fun" function ;
  varDecl        ? "var" IDENTIFIER ( "=" expression )? ";" ;


  statement      ? exprStmt
                 | forStmt
                 | ifStmt
                 | printStmt
                 | returnStmt
                 | whileStmt
                 | block ;


  exprStmt       ? expression ";" ;
  forStmt        ? "for" "(" ( varDecl | exprStmt | ";" )
                             expression? ";"
                             expression? ")" statement ;
  ifStmt         ? "if" "(" expression ")" statement ( "else" statement )? ;
  printStmt      ? "print" expression ";" ;
  returnStmt     ? "return" expression? ";" ;
  whileStmt      ? "while" "(" expression ")" statement ;
  block          ? "{" declaration* "}" ;


  expression     ? assignment ;

  assignment     ? ( call "." )? IDENTIFIER "=" assignment
                 | logic_or;


  logic_or       ? logic_and ( "or" logic_and )* ;
  logic_and      ? equality ( "and" equality )* ;
  equality       ? comparison ( ( "!=" | "==" ) comparison )* ;
  comparison     ? addition ( ( ">" | ">=" | "<" | "<=" ) addition )* ;
  addition       ? multiplication ( ( "-" | "+" ) multiplication )* ;
  multiplication ? unary ( ( "/" | "*" ) unary )* ;


  unary          ? ( "!" | "-" ) unary | call ;
  call           ? primary ( "(" arguments? ")" | "." IDENTIFIER )* ;
  primary        ? "true" | "false" | "nil" | "this"
                 | NUMBER | STRING | IDENTIFIER | "(" expression ")"
                 | "super" "." IDENTIFIER ;


  function       ? IDENTIFIER "(" parameters? ")" block ;
  parameters     ? IDENTIFIER ( "," IDENTIFIER )* ;
  arguments      ? expression ( "," expression )* ;


  NUMBER         ? DIGIT+ ( "." DIGIT+ )? ;
  STRING         ? '"' <any char except '"'>* '"' ;
  IDENTIFIER     ? ALPHA ( ALPHA | DIGIT )* ;
  ALPHA          ? 'a' ... 'z' | 'A' ... 'Z' | '_' ;
  DIGIT          ? '0' ... '9' ;

*/


typedef enum {
  PREC_NONE,        // lowest = do last
  PREC_ASSIGNMENT,  // =
  PREC_CONDITIONAL, // ?:
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_BIN_OR,      // |
  PREC_BIN_XOR,     // ^
  PREC_BIN_AND,     // &
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_SHIFT,       // << >>
  PREC_TERM,        // + -
  PREC_FACTOR,      // * / %
  PREC_UNARY,       // ! - ~
  PREC_SUBSCRIPT,   // []
  PREC_CALL,        // . ()
  PREC_PRIMARY      // highest = do first
} Precedence;

typedef void (*ParseFn)(VM* vm, bool canAssign);

// Moved to parser.h
/*
typedef struct Parser {
  bool hadError;
  bool panicMode;
  Token current;
  Token previous;
} Parser;
*/

typedef struct {
  ParseFn prefix; // function to compile a prefix expr starting with that token
  ParseFn infix; // function to compile an infix expr whose left operand is followed by that token
  Precedence precedence; // precedence of an infix expression that uses that token as an operator
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_SCRIPT
} FunctionType;


typedef struct Compiler {
  struct Compiler* enclosing;
  ObjFunction* function;
  FunctionType type;

  Parser* parser;

  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvalues[UINT8_COUNT];
  int scopeDepth;

  ErrorCb error_callback;

} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing; // Lexical enclosure, not inheritance
  Token name;
  bool hasSuperclass;
} ClassCompiler;

// Parser parser; // moved to parser.h
// Compiler* current = NULL; // Moved to vm.c
// ClassCompiler* currentClass = NULL; // Moved to vm.c

// https://github.com/munificent/craftinginterpreters/blob/master/note/answers/chapter23_jumping/2.md
int innermostLoopStart = -1;
int innermostLoopScopeDepth = 0;
int innermostBreakScopeStart = -1;
int innermostBreakScopeDepth = 0;
int* innermostBreakJump = NULL;
int innermostBreakJumps = 0;

static Chunk* currentChunk(VM* vm) {
  return &vm->compiler->function->chunk;
  //return &current->function->chunk;
}



static void emitByte(VM* vm, uint8_t byte) {
#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:emitByte(%d)\n", byte);
#endif // DEBUG_TRACE_COMPILER
  writeChunk(vm, currentChunk(vm), byte, vm->compiler->parser->previous.fileno, vm->compiler->parser->previous.lineno, vm->compiler->parser->previous.charno);
}

static void emitWord(VM* vm, uint16_t word) {
#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:emitWord(%d)\n", word);
#endif // DEBUG_TRACE_COMPILER
  writeChunk(vm, currentChunk(vm), word>>8, vm->compiler->parser->previous.fileno, vm->compiler->parser->previous.lineno, vm->compiler->parser->previous.charno);
  writeChunk(vm, currentChunk(vm), word&0xff, vm->compiler->parser->previous.fileno, vm->compiler->parser->previous.lineno, vm->compiler->parser->previous.charno);
}

static void emitBytes(VM* vm, uint8_t byte1, uint8_t byte2) {
  emitByte(vm, byte1);
  emitByte(vm, byte2);
}

// Unconditional jump backward (to a known address)
static void emitLoop(VM* vm, int loopStart) {
  emitByte(vm, OP_LOOP);

  int offset = currentChunk(vm)->count - loopStart + 2;
  if (offset > UINT16_MAX) error(vm->compiler->parser, "Loop body too large.");

  emitByte(vm, (offset >> 8) & 0xff);
  emitByte(vm, offset & 0xff);
}

// Jump forward (to an as of yet unknown address)
static int emitJump(VM* vm, uint8_t instruction) {
  emitByte(vm, instruction);
  emitByte(vm, 0xff); // Placeholder address
  emitByte(vm, 0xff);
  return currentChunk(vm)->count - 2; // Placeholder position
}

static void emitReturn(VM* vm) {
  //printf("compiler:emitReturn() called\n");
  //if (current->type == TYPE_INITIALIZER) {
  if (vm->compiler->type == TYPE_INITIALIZER) {
    //printf("compiler:emitReturn() type is TYPE_INITIALIZER\n");
    //emitBytes(OP_GET_LOCAL, 0);
    emitByte(vm, OP_GET_LOCAL);
    emitWord(vm, 0);
    // Stack slot zero contains the instance
  } else {
    //printf("compiler:emitReturn() will return NULL\n");
    emitByte(vm, OP_NULL);
  }
  emitByte(vm, OP_RETURN);
}

static uint16_t makeConstant(VM* vm, Value value) {
  int constant = addConstant(vm, currentChunk(vm), value);
//  if (constant > UINT8_MAX) {
  if (constant > UINT16_MAX) {
    error(vm->compiler->parser, "Too many constants in one chunk.");
    return 0;
  }

//  return (uint8_t)constant;
  return (uint16_t)constant;
}

static void emitConstant(VM* vm, Value value) {
//  emitBytes(vm, OP_CONSTANT, makeConstant(vm, value));
  emitByte(vm, OP_CONSTANT);
  uint16_t constant = makeConstant(vm, value);
  emitWord(vm, constant);
}

static void patchJump(VM* vm, int offset) {
  (unused)vm;
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = currentChunk(vm)->count - offset - 2;

  if (jump > UINT16_MAX) {
    error(vm->compiler->parser, "Too much code to jump over.");
  }

  currentChunk(vm)->code[offset] = (jump >> 8) & 0xff;
  currentChunk(vm)->code[offset + 1] = jump & 0xff;
}


// TODO: Turn this into a proper constructor
static void initCompiler(VM* vm, Compiler* compiler, Parser* parser, FunctionType type) {
#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:initCompiler(%p, %d)\n", compiler, type);
#endif // DEBUG_TRACE_COMPILER
  compiler->enclosing = vm->compiler; // current;
  compiler->parser = parser;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->error_callback = vm->error_callback;

#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:initCompiler() calling newFunction()\n");
#endif // DEBUG_TRACE_COMPILER
  compiler->function = newFunction(vm);
#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:initCompiler() newFunction() returned %p\n", compiler->function);
#endif // DEBUG_TRACE_COMPILER
//  current = compiler;
  vm->compiler = compiler;

  if (type != TYPE_SCRIPT) {
#ifdef DEBUG_TRACE_COMPILER
    printf("compiler:initCompiler() type not TYPE_SCRIPT\n");
#endif // DEBUG_TRACE_COMPILER
//    current->function->name = copyString(vm, vm->compiler->parser->previous.start,
    vm->compiler->function->name = copyString(vm, vm->compiler->parser->previous.start,
                                         vm->compiler->parser->previous.length);
  }

//  Local* local = &current->locals[current->localCount++];
  Local* local = &vm->compiler->locals[vm->compiler->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  if (type != TYPE_FUNCTION) {
    local->name.start = "this";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }

#ifdef DEBUG_TRACE_COMPILER
//  printf("compiler:initCompiler() compiler %p initialized\n", current);
  printf("compiler:initCompiler() compiler %p initialized\n", vm->compiler);
#endif // DEBUG_TRACE_COMPILER
}

static ObjFunction* endCompiler(VM* vm) {
#ifdef DEBUG_TRACE_COMPILER
//  printf("compiler:endCompiler() %p finished compiling function %p\n", current, current->function);
  printf("compiler:endCompiler() %p finished compiling function %p\n", vm->compiler, vm->compiler->function);
#endif // DEBUG_TRACE_COMPILER
  emitReturn(vm);
//  ObjFunction* function = current->function;
  ObjFunction* function = vm->compiler->function;
#ifdef DEBUG_PRINT_CODE
  if (!vm->compiler->parser->hadError) {
    hexdump(currentChunk(vm)->code, currentChunk(vm)->count);
    disassembleChunk(currentChunk(vm),
        function->name != NULL ? function->name->chars : "<script>");
  }
#endif

//  current = current->enclosing;
  vm->compiler = vm->compiler->enclosing;
  return function;
}

static void beginScope(VM* vm) {
//  current->scopeDepth++;
  vm->compiler->scopeDepth++;
}

static void endScope(VM* vm) {
//  current->scopeDepth--;
  struct Compiler* cc = vm->compiler;
  cc->scopeDepth--;

  //uint8_t count = 0;
  while (cc->localCount > 0 &&
         cc->locals[cc->localCount - 1].depth >
            cc->scopeDepth) {
    if (cc->locals[cc->localCount - 1].isCaptured) {
      emitByte(vm, OP_CLOSE_UPVALUE);
    } else {
      emitByte(vm, OP_POP);
      //count++; // Custom OP_POPN counter init
    }

    cc->localCount--;
  }

  // Custom OP_POPN code
  /*
  if (count == 0) {
    return; // None
  } else if (count == 1) {
    emitByte(OP_POP); // Just one
  } else {
    emitBytes(OP_POPN, count); // Pop (count) values off the stack
  }
  */

}

// Forward declarations so the grammar handlers can reach the parser
static void expression(VM* vm);
static void statement(VM* vm);
static void declaration(VM* vm);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(VM* vm, Precedence precedence);

//static uint8_t identifierConstant(VM* vm, Token* name) {
static uint16_t identifierConstant(VM* vm, Token* name) {
  return makeConstant(vm, OBJ_VAL(copyString(vm, name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(VM* vm, Compiler* compiler, Token* name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
        error(vm->compiler->parser, "Cannot read local variable in its own initializer.");
      }
      return i;
    }
  }

  return -1;
}

static int addUpvalue(VM* vm, Compiler* compiler, uint8_t index, bool isLocal) {
  int upvalueCount = compiler->function->upvalueCount;

  for (int i = 0; i < upvalueCount; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_COUNT) {
    error(vm->compiler->parser, "Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(VM* vm, Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(vm, compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpvalue(vm, compiler, (uint8_t)local, true);
  }

  int upvalue = resolveUpvalue(vm, compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(vm, compiler, (uint8_t)upvalue, false);
  }

  return -1;
}

static void addLocal(VM* vm, Token name) {
//  if (current->localCount == UINT8_COUNT) {
  if (vm->compiler->localCount == UINT8_COUNT) {
    error(vm->compiler->parser, "Too many local variables in function.");
    return;
  }

//  Local* local = &current->locals[current->localCount++];
  Local* local = &vm->compiler->locals[vm->compiler->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

static void declareVariable(VM* vm) {
  // Global variables are implicitly declared.
//  if (current->scopeDepth == 0) return;
  if (vm->compiler->scopeDepth == 0) return;

  Token* name = &vm->compiler->parser->previous;
//  for (int i = current->localCount - 1; i >= 0; i--) {
  for (int i = vm->compiler->localCount - 1; i >= 0; i--) {
//    Local* local = &current->locals[i];
    Local* local = &vm->compiler->locals[i];
//    if (local->depth != -1 && local->depth < current->scopeDepth) {
    if (local->depth != -1 && local->depth < vm->compiler->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      error(vm->compiler->parser, "Variable with this name already declared in this scope.");
    }
  }

  addLocal(vm, *name);
}

//static uint8_t parseVariable(VM* vm, const char* errorMessage) {
static uint16_t parseVariable(VM* vm, const char* errorMessage) {
  consume(vm->compiler->parser, TOKEN_IDENTIFIER, errorMessage);

  declareVariable(vm);
//  if (current->scopeDepth > 0) return 0;
  if (vm->compiler->scopeDepth > 0) return 0;

  return identifierConstant(vm, &vm->compiler->parser->previous);
}

static void markInitialized(VM* vm) {
//  if (current->scopeDepth == 0) return;
//  current->locals[current->localCount - 1].depth = current->scopeDepth;
  if (vm->compiler->scopeDepth == 0) return;
  vm->compiler->locals[vm->compiler->localCount - 1].depth = vm->compiler->scopeDepth;
}

//static void defineVariable(VM* vm, uint8_t global) {
static void defineVariable(VM* vm, uint16_t global) {
  //printf("compiler:defineVariable() vm=%p global=%d\n", vm, global);
//  if (current->scopeDepth > 0) {
  if (vm->compiler->scopeDepth > 0) {
    markInitialized(vm);
    return;
  }
//  emitBytes(vm, OP_DEFINE_GLOBAL, global);
  emitByte(vm, OP_DEFINE_GLOBAL);
  emitWord(vm, global);
}

static uint8_t argumentList(VM* vm) {
  uint8_t argCount = 0;
  if (!check(vm->compiler->parser, TOKEN_RIGHT_PAREN)) {
    do {
      expression(vm);

      if (argCount == 255) {
        error(vm->compiler->parser, "Cannot have more than 255 arguments.");
      }
      argCount++;
    } while (match(vm->compiler->parser, TOKEN_COMMA));
  }

  consume(vm->compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}

static void and_(VM* vm, bool canAssign) {
  (unused)canAssign;
  int endJump = emitJump(vm, OP_QJMP_IF_FALSE);

  emitByte(vm, OP_POP);
  parsePrecedence(vm, PREC_AND);

  patchJump(vm, endJump);
}

static void binary(VM* vm, bool canAssign) {
  (unused)canAssign;
  // Remember the operator.
  TokenType operatorType = vm->compiler->parser->previous.type;

  // Compile the right operand.
  ParseRule* rule = getRule(operatorType); // asks: what precedence is this operator?
  parsePrecedence(vm, (Precedence)(rule->precedence + 1)); // ok, look for higher ones

  // No higher level expressions found?
  // The stack now contains our two (computed or constant) operands
  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG_EQUAL:      emitByte(vm, OP_NEQUAL); break;
    case TOKEN_EQUAL_EQUAL:     emitByte(vm, OP_EQUAL); break;
    case TOKEN_GREATER:         emitByte(vm, OP_GREATER); break;
    case TOKEN_GREATER_EQUAL:   emitByte(vm, OP_GEQUAL); break;
    case TOKEN_LESS:            emitByte(vm, OP_LESS); break;
    case TOKEN_LESS_EQUAL:      emitByte(vm, OP_LEQUAL); break;
    case TOKEN_PLUS:            emitByte(vm, OP_ADD); break;
    case TOKEN_MINUS:           emitByte(vm, OP_SUBTRACT); break;
    case TOKEN_STAR:            emitByte(vm, OP_MULTIPLY); break;
    case TOKEN_SLASH:           emitByte(vm, OP_DIVIDE); break;
    case TOKEN_PERCENT:         emitByte(vm, OP_MODULO); break;
    case TOKEN_GREATER_GREATER: emitByte(vm, OP_BIN_SHIFTR); break;
    case TOKEN_LESS_LESS:       emitByte(vm, OP_BIN_SHIFTL); break;
    case TOKEN_AMP:             emitByte(vm, OP_BIN_AND); break;
    case TOKEN_CARET:           emitByte(vm, OP_BIN_XOR); break;
    case TOKEN_PIPE:            emitByte(vm, OP_BIN_OR); break;
    default:
      return; // Unreachable.
  }
}

static void call(VM* vm, bool canAssign) {
  (unused)canAssign;
  uint8_t argCount = argumentList(vm);
  emitBytes(vm, OP_CALL, argCount);
}

static void dot(VM* vm, bool canAssign) {
  consume(vm->compiler->parser, TOKEN_IDENTIFIER, "Expect property name after '.'.");
//  uint8_t name = identifierConstant(vm, &vm->compiler->parser->previous);
  uint16_t name = identifierConstant(vm, &vm->compiler->parser->previous);


  if (canAssign && match(vm->compiler->parser, TOKEN_EQUAL)) {
    expression(vm);
//    emitBytes(vm, OP_SET_PROPERTY, name);
    emitByte(vm, OP_SET_PROPERTY);
    emitWord(vm, name);
#ifdef OPTIMIZE_METHOD_CALLS
  } else if (match(vm->compiler->parser, TOKEN_LEFT_PAREN)) {
    // Optimized invocation of bound method calls - chapter 28.5
    // .method is followed by () = call that method instead of loading it on the stack
    // This is the most common way to use a method, but not the ONLY way
    uint8_t argCount = argumentList(vm);
    //emitBytes(OP_INVOKE, name);
    emitByte(vm, OP_INVOKE);
    emitWord(vm, name);
    emitByte(vm, argCount);
  #endif
  } else {
//    emitBytes(vm, OP_GET_PROPERTY, name);
    emitByte(vm, OP_GET_PROPERTY);
    emitWord(vm, name);
  }


  /*
  // This section will likely expand greatly when
  // methods, getters and setters are introduced
  // Custom: Similar pattern to named variables
  uint8_t setOp = OP_SET_PROPERTY;
  uint8_t getOp = OP_GET_PROPERTY;

  // The code below does not work for attributes because the
  // "get" OP pops the class instance off the stack causing the
  // subsequent "set" to fail with "only instances have fields"
  // For simple inc/dec this could be solved with dedicated
  // get/modify/set ops for fields, but this scales poorly
  // and it would not solve +=, -=, *= or /=

  // Experimental support for ++, +=, -- an -=
  if (canAssign) {
    if (match(vm->compiler->parser, TOKEN_EQUAL)) {
      expression(vm);
      emitBytes(setOp, (uint8_t)name);
    } else if (match(vm->compiler->parser, TOKEN_PLUS_PLUS)) {
      emitBytes(getOp, (uint8_t)name);
      emitByte(OP_INC);
      emitBytes(setOp, (uint8_t)name);
      emitByte(OP_DEC); // Suffix increment, return previous value
    } else if (match(vm->compiler->parser, TOKEN_PLUS_EQUAL)) {
      emitBytes(getOp, (uint8_t)name);
      expression(vm);
      emitByte(OP_ADD);
      emitBytes(setOp, (uint8_t)name);
    } else if (match(vm->compiler->parser, TOKEN_MINUS_MINUS)) {
      emitBytes(getOp, (uint8_t)name);
      emitByte(OP_DEC);
      emitBytes(setOp, (uint8_t)name);
      emitByte(OP_INC); // Suffix decrement, return previous value
    } else if (match(vm->compiler->parser, TOKEN_MINUS_EQUAL)) {
      emitBytes(getOp, (uint8_t)name);
      expression(vm);
      emitByte(OP_SUBTRACT);
      emitBytes(setOp, (uint8_t)name);
    } else {
      emitBytes(getOp, (uint8_t)name);
    }
  } else {
    emitBytes(getOp, (uint8_t)name);
  }
  */
}

static void literal(VM* vm, bool canAssign) {
  (unused)canAssign;
  switch (vm->compiler->parser->previous.type) {
    case TOKEN_FALSE: emitByte(vm, OP_FALSE); break;
    case TOKEN_NULL: emitByte(vm, OP_NULL); break;
    case TOKEN_TRUE: emitByte(vm, OP_TRUE); break;
    default:
      return; // Unreachable.
  }
}


// Infix [ operator:
// Subscript [index] or slice [offset:length] of an array or string -- EXPERIMENTAL
static void subscr(VM* vm, bool canAssign) {
  int expressions = 0;
  bool colon = false;
  while (!check(vm->compiler->parser, TOKEN_RIGHT_BRACKET)) {
    if (match(vm->compiler->parser, TOKEN_COLON)) {
      if (colon == true) {
        error(vm->compiler->parser, "Expect only one ':' character. (#subscr)");
        return;
      }
      colon = true;
      if (expressions == 0) {
        //printf("compiler:subscr() offset is NULL\n");
        emitByte(vm, OP_NULL); // Offset is NULL
        expressions++;
      }
    }
    if (check(vm->compiler->parser, TOKEN_RIGHT_BRACKET)) break;
    expression(vm);
    expressions++;
    if (expressions == 2) break; // We have both an offset and a length
  }
  consume(vm->compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after subscript. (#subscr)");
  if (colon == true && expressions == 1) {
    // We found a colon but only one expression, length must be NULL
    //printf("compiler:subscr() length is NULL\n");
    emitByte(vm, OP_NULL);
  }

  // Now that we have parsed the subscript/slice, let's see which one it is
  // so we can emit the right Opcode
  if (colon) {
    if (canAssign && match(vm->compiler->parser, TOKEN_EQUAL)) {
      expression(vm);
      emitByte(vm, OP_SET_SLICE);
    } else {
      emitByte(vm, OP_GET_SLICE);
    }
  } else {
    if (canAssign && match(vm->compiler->parser, TOKEN_EQUAL)) {
      expression(vm);
      emitByte(vm, OP_SET_INDEX);
    } else {
      emitByte(vm, OP_GET_INDEX);
    }
  }
}


// Prefix [ operator:
// Array initializer [] with zero or more expressions
static void array(VM* vm, bool canAssign) {
  (unused)canAssign;
  int length = 0;
  if (!check(vm->compiler->parser, TOKEN_RIGHT_BRACKET)) {
    do {
      if (match(vm->compiler->parser, TOKEN_RIGHT_BRACKET)) break; // Ignore lingering comma
      expression(vm);
      length++;
      if (length > 255) {
        error(vm->compiler->parser, "An array constant can not have more than 255 elements.");
        return;
      }
    } while (match(vm->compiler->parser, TOKEN_COMMA));
  }
  consume(vm->compiler->parser, TOKEN_RIGHT_BRACKET, "Expect ']' after array elements. (#array)");
  emitBytes(vm, OP_MAKE_ARRAY, length);
}


static void grouping(VM* vm, bool canAssign) {
  (unused)canAssign;
  expression(vm);
  consume(vm->compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}


static void b2number(VM* vm, bool canAssign) {
  (unused)canAssign;
//  double value = (double) strtol(vm->compiler->parser->previous.start+2, NULL, 2); // +2 = skip '0b' prefix
  double value = str_to_double(vm->compiler->parser->previous.start+2, vm->compiler->parser->previous.length-2, 2); // +2 = skip '0b' prefix
  emitConstant(vm, NUMBER_VAL(value));
}


static void b8number(VM* vm, bool canAssign) {
  (unused)canAssign;
//  double value = (double) strtol(vm->compiler->parser->previous.start, NULL, 8);
  double value = str_to_double(vm->compiler->parser->previous.start, vm->compiler->parser->previous.length, 8);
  emitConstant(vm, NUMBER_VAL(value));
}


static void b10number(VM* vm, bool canAssign) {
  (unused)canAssign;
//  double value = strtod(vm->compiler->parser->previous.start, NULL);
  double value = str_to_double(vm->compiler->parser->previous.start, vm->compiler->parser->previous.length, 10);
  emitConstant(vm, NUMBER_VAL(value));
}


static void b16number(VM* vm, bool canAssign) {
  (unused)canAssign;
//  double value = (double) strtol(vm->compiler->parser->previous.start+2, NULL, 16); // +2 = skip '0x' prefix
  double value = str_to_double(vm->compiler->parser->previous.start+2, vm->compiler->parser->previous.length-2, 16); // +2 = skip '0x' prefix
  emitConstant(vm, NUMBER_VAL(value));
}


static void or_(VM* vm, bool canAssign) {
  (unused)canAssign;
  int elseJump = emitJump(vm, OP_QJMP_IF_FALSE);
  int endJump = emitJump(vm, OP_JUMP);

  patchJump(vm, elseJump);
  emitByte(vm, OP_POP);

  parsePrecedence(vm, PREC_OR);
  patchJump(vm, endJump);
}



/**
 * Encode a code point using UTF-8
 *
 * @author Ondrej Hruska <ondra@ondrovo.com>
 * @license MIT
 *
 * @param out - output buffer (min 5 characters), will be 0-terminated
 * @param utf - code point 0-0x10FFFF
 * @return number of bytes on success, 0 on failure (also produces U+FFFD, which uses 3 bytes)
 */
int utf8_encode(char *out, uint32_t utf) {
  if (utf <= 0x7F) {
    // Plain ASCII
    out[0] = (char) utf;
    out[1] = 0;
    return 1;
  }
  else if (utf <= 0x07FF) {
    // 2-byte unicode
    out[0] = (char) (((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char) (((utf >> 0) & 0x3F) | 0x80);
    out[2] = 0;
    return 2;
  }
  else if (utf <= 0xFFFF) {
    // 3-byte unicode
    out[0] = (char) (((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char) (((utf >>  6) & 0x3F) | 0x80);
    out[2] = (char) (((utf >>  0) & 0x3F) | 0x80);
    out[3] = 0;
    return 3;
  }
  else if (utf <= 0x10FFFF) {
    // 4-byte unicode
    out[0] = (char) (((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char) (((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char) (((utf >>  6) & 0x3F) | 0x80);
    out[3] = (char) (((utf >>  0) & 0x3F) | 0x80);
    out[4] = 0;
    return 4;
  }
  else {
    // error - use replacement character
    out[0] = (char) 0xEF;
    out[1] = (char) 0xBF;
    out[2] = (char) 0xBD;
    out[3] = 0;
    return 0;
  }
}

static void string(VM* vm, bool canAssign) {
  (unused)canAssign;
  int i_len = vm->compiler->parser->previous.length - 2; // -2: Leading and trailing quotes
  //printf("compiler:string() i_len=%d\n", i_len);
  const char* i_str = vm->compiler->parser->previous.start + 1; // +1: Skip the leading quote
  char* o_str = malloc(i_len*sizeof(char) + 1);
  int i=0;
  int o=0;
  while (i<i_len) {
    // Look 5 ahead?
    if (i<i_len-5) {
      if (i_str[i] == '\\' && i_str[i+1] == 'u') {
        // \uFFFF
        //char* buf = (char*) i_str+i+2;
        int codepoint = (int) strtol((char*) i_str+i+2, NULL, 16);
        i+=6;
        o+=utf8_encode(&o_str[o], codepoint);
        continue;
      }
    }
    // Look 3 ahead?
    if (i<i_len-3) {
      if (i_str[i] == '\\' && i_str[i+1] == 'x') {
        // \xFF
        //char* buf = (char*) i_str+i+2;
        int ascii = (int) strtol((char*) i_str+i+2, NULL, 16);
        o_str[o] = (char) ascii;
        i+=4;
        o+=1;
        continue;
      }
    }
    // Look 1 ahead?
    if (i<i_len-1 && i_str[i] == '\\') {
      switch (i_str[i+1]) {
        case '\\': o_str[o] = '\\'; i+=2; o+=1; continue;
        case '"':  o_str[o] = '"';  i+=2; o+=1; continue;
        case '\'': o_str[o] = '\''; i+=2; o+=1; continue;
        case 'a':  o_str[o] = '\a'; i+=2; o+=1; continue; // bell
        case 'b':  o_str[o] = '\b'; i+=2; o+=1; continue; // backspace
        case 'e':  o_str[o] = '\e'; i+=2; o+=1; continue; // escape
        case 'n':  o_str[o] = '\n'; i+=2; o+=1; continue; // newline
        case 'r':  o_str[o] = '\r'; i+=2; o+=1; continue; // carriage return
        case 't':  o_str[o] = '\t'; i+=2; o+=1; continue; // tab
        case '?':  o_str[o] = '?';  i+=2; o+=1; continue; // question mark
      }
    }
    // Copy char as-is
    o_str[o] = i_str[i];
    i++;
    o++;
  }
  o_str[o] = '\0';

  //emitConstant(OBJ_VAL(copyString(vm->compiler->parser->previous.start + 1,
  //                                vm->compiler->parser->previous.length - 2)));
  Value string_obj = OBJ_VAL(copyString(vm, o_str, o));
  push(vm, string_obj);
  emitConstant(vm, string_obj);
  pop(vm);
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
  printf("compiler:string() free(%p) // temp buffer\n", (void*)o_str );
#endif // DEBUG_TRACE_MEMORY_VERBOSE
  free(o_str);
}


static void assignmentOperator(VM* vm, uint8_t getOp, uint8_t setOp, uint8_t numOp, int arg) {
  emitByte(vm, getOp);
  emitWord(vm, arg);
  expression(vm);
  emitByte(vm, numOp);
  emitByte(vm, setOp);
  emitWord(vm, arg);
}

static void namedVariable(VM* vm, Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(vm, vm->compiler, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(vm, vm->compiler, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(vm, &name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }


  // Original code
//  if (canAssign && match(vm->compiler->parser, TOKEN_EQUAL)) {
//    expression(vm);
//    emitBytes(setOp, (uint8_t)arg);
//  } else {
//    emitBytes(getOp, (uint8_t)arg);
//  }

  // Experimental support for assignment operators (++, +=, --, -= etc.)
  if (canAssign) {
    if (match(vm->compiler->parser, TOKEN_EQUAL)) {
      expression(vm);
      emitByte(vm, setOp);
      emitWord(vm, arg);
    } else if (match(vm->compiler->parser, TOKEN_PLUS_PLUS)) {
      emitByte(vm, getOp);
      emitWord(vm, arg);
      emitByte(vm, OP_INC);
      emitByte(vm, setOp);
      emitWord(vm, arg);
      emitByte(vm, OP_DEC); // Note: Suffix increment, return previous value
    } else if (match(vm->compiler->parser, TOKEN_MINUS_MINUS)) {
      emitByte(vm, getOp);
      emitWord(vm, arg);
      emitByte(vm, OP_DEC);
      emitByte(vm, setOp);
      emitWord(vm, arg);
      emitByte(vm, OP_INC); // Note: Suffix decrement, return previous value
    } else if (match(vm->compiler->parser, TOKEN_PLUS_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_ADD, arg);
    } else if (match(vm->compiler->parser, TOKEN_MINUS_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_SUBTRACT, arg);
    } else if (match(vm->compiler->parser, TOKEN_STAR_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_MULTIPLY, arg);
    } else if (match(vm->compiler->parser, TOKEN_SLASH_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_DIVIDE, arg);
    } else if (match(vm->compiler->parser, TOKEN_PERCENT_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_MODULO, arg);
    } else if (match(vm->compiler->parser, TOKEN_CARET_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_BIN_XOR, arg);
    } else if (match(vm->compiler->parser, TOKEN_AMP_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_BIN_AND, arg);
    } else if (match(vm->compiler->parser, TOKEN_PIPE_EQUAL)) {
      assignmentOperator(vm, getOp, setOp, OP_BIN_OR, arg);
    } else {
      emitByte(vm, getOp);
      emitWord(vm, arg);
    }
  } else {
    emitByte(vm, getOp);
    emitWord(vm, arg);
  }
}

static void variable(VM* vm, bool canAssign) {
  (unused)canAssign;
  namedVariable(vm, vm->compiler->parser->previous, canAssign);
}


static Token syntheticToken(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  // FIXME: Definitely not best practice to leave the remaining fields uninitialized
  return token;
}


static void super_(VM* vm, bool canAssign) {
  if (vm->currentClass == NULL) {
    error(vm->compiler->parser, "Cannot use 'super' outside of a class.");
  } else if (!vm->currentClass->hasSuperclass) {
    error(vm->compiler->parser, "Cannot use 'super' in a class with no superclass.");
  }
  consume(vm->compiler->parser, TOKEN_DOT, "Expect '.' after 'super'.");
  consume(vm->compiler->parser, TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint16_t name = identifierConstant(vm, &vm->compiler->parser->previous);

  namedVariable(vm, syntheticToken("this"), false);
  namedVariable(vm, syntheticToken("super"), false);
  emitByte(vm, OP_GET_SUPER);
  emitWord(vm, name);
}


static void this_(VM* vm, bool canAssign) {
  (unused)canAssign;
  if (vm->currentClass == NULL) {
    error(vm->compiler->parser, "Cannot use 'this' outside of a class.");
    return;
  }
  variable(vm, false); // false = user can not assign to 'this'
  // see initCompiler() for initialization
}


static void unary(VM* vm, bool canAssign) {
  (unused)canAssign;
  TokenType operatorType = vm->compiler->parser->previous.type;

  // Compile the operand.
  parsePrecedence(vm, PREC_UNARY);

  // Emit the operator instruction.
  switch (operatorType) {
    case TOKEN_BANG: emitByte(vm, OP_NOT); break;
    case TOKEN_MINUS: emitByte(vm, OP_NEGATE); break;
    case TOKEN_TILDE: emitByte(vm, OP_BIN_NOT); break;
    //case TOKEN_PLUS_PLUS: emitByte(OP_INC); break;   // Won't work, needs implicit assignment
    //case TOKEN_MINUS_MINUS: emitByte(OP_DEC); break; // Won't work, needs implicit assignment
    default:
      return; // Unreachable.
  }
}


static void ternary(VM* vm, bool canAssign) { // aka. conditional()
  (unused)canAssign;
  int thenJump = emitJump(vm, OP_PJMP_IF_FALSE);
  // Compile the then branch.
  parsePrecedence(vm, PREC_CONDITIONAL);
  consume(vm->compiler->parser, TOKEN_COLON, "Expect ':' after then branch of conditional operator.");
  int elseJump = emitJump(vm, OP_JUMP); // Mark end of THEN block
  patchJump(vm, thenJump);
  // Compile the else branch.
  parsePrecedence(vm, PREC_ASSIGNMENT);
  patchJump(vm, elseJump);
}




// Vaughan Pratt parser table -- NULL/PREC_NONE means token is not used in expressions (yet)
// The number and order of parse rules *MUST* match
// typedef TokenType exactly or weird things will happen!
// (see scanner.h)
ParseRule rules[] = {
  // Prefix   Infix    Precedence
                                          // Single-character tokens.
  { grouping, call,    PREC_CALL },       // TOKEN_LEFT_PAREN
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_PAREN
  { NULL,     NULL,    PREC_NONE },       // TOKEN_LEFT_BRACE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACE
  { array,    subscr,  PREC_SUBSCRIPT },  // TOKEN_LEFT_BRACKET
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RIGHT_BRACKET
  { NULL,     NULL,    PREC_NONE },       // TOKEN_COMMA
  { NULL,     dot,     PREC_CALL },       // TOKEN_DOT
  { unary,    binary,  PREC_TERM },       // TOKEN_MINUS
  { NULL,     binary,  PREC_TERM },       // TOKEN_PLUS
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SEMICOLON
  { NULL,     ternary, PREC_CONDITIONAL },// TOKEN_QUESTION
  { NULL,     NULL,    PREC_NONE },       // TOKEN_COLON
  { NULL,     NULL,    PREC_NONE },       // TOKEN_HASH
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_PERCENT
  { NULL,     NULL,    PREC_FACTOR },     // TOKEN_PERCENT_EQUAL
                                          // One or two character tokens.
  { unary,    NULL,    PREC_UNARY },      // TOKEN_TILDE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_TILDE_EQUAL
  { NULL,     binary,  PREC_BIN_AND },    // TOKEN_AMP
  { NULL,     NULL,    PREC_NONE },       // TOKEN_AMP_EQUAL
  { NULL,     binary,  PREC_BIN_XOR },    // TOKEN_CARET
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CARET_EQUAL
  { NULL,     binary,  PREC_BIN_OR },     // TOKEN_PIPE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_PIPE_EQUAL
  { unary,    NULL,    PREC_NONE },       // TOKEN_BANG
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_BANG_EQUAL
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EQUAL
  { NULL,     binary,  PREC_EQUALITY },   // TOKEN_EQUAL_EQUAL
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_GREATER_EQUAL
  { NULL,     binary,  PREC_SHIFT },      // TOKEN_GREATER_GREATER
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS
  { NULL,     binary,  PREC_COMPARISON }, // TOKEN_LESS_EQUAL
  { NULL,     binary,  PREC_SHIFT },      // TOKEN_LESS_LESS
  { NULL,     NULL,    PREC_NONE },       // TOKEN_PLUS_PLUS
  { NULL,     NULL,    PREC_NONE },       // TOKEN_PLUS_EQUAL
  { NULL,     NULL,    PREC_NONE },       // TOKEN_MINUS_MINUS
  { NULL,     NULL,    PREC_NONE },       // TOKEN_MINUS_EQUAL
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_STAR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_STAR_EQUAL
  { NULL,     binary,  PREC_FACTOR },     // TOKEN_SLASH
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SLASH_EQUAL
                                          // Literals.
  { variable, NULL,    PREC_NONE },       // TOKEN_IDENTIFIER
  { string,   NULL,    PREC_NONE },       // TOKEN_STRING
  { b2number, NULL,    PREC_NONE },       // TOKEN_BASE2_NUMBER
  { b8number, NULL,    PREC_NONE },       // TOKEN_BASE8_NUMBER
  { b10number,NULL,    PREC_NONE },       // TOKEN_BASE10_NUMBER
  { b16number,NULL,    PREC_NONE },       // TOKEN_BASE16_NUMBER
                                          // Keywords.
  { NULL,     and_,    PREC_AND },        // TOKEN_AND
  { NULL,     NULL,    PREC_NONE },       // TOKEN_BREAK
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CASE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CLASS
  { NULL,     NULL,    PREC_NONE },       // TOKEN_CONTINUE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_DEFAULT
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ELSE
  { literal,  NULL,    PREC_NONE },       // TOKEN_FALSE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FOR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_FUN
  { NULL,     NULL,    PREC_NONE },       // TOKEN_IF
  { literal,  NULL,    PREC_NONE },       // TOKEN_NULL
  { NULL,     or_,     PREC_OR },         // TOKEN_OR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_PRINT
  { NULL,     NULL,    PREC_NONE },       // TOKEN_RETURN
  { super_,   NULL,    PREC_NONE },       // TOKEN_SUPER
  { NULL,     NULL,    PREC_NONE },       // TOKEN_SWITCH
  { this_,    NULL,    PREC_NONE },       // TOKEN_THIS
  { literal,  NULL,    PREC_NONE },       // TOKEN_TRUE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_VAR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_WHILE
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EXIT
                                          // Internal.
  { NULL,     NULL,    PREC_NONE },       // TOKEN_ERROR
  { NULL,     NULL,    PREC_NONE },       // TOKEN_EOF
};

// Will only parse expressions HIGHER than specified
static void parsePrecedence(VM* vm, Precedence precedence) {
  advance(vm->compiler->parser);
  ParseFn prefixRule = getRule(vm->compiler->parser->previous.type)->prefix;
  //printf("prefixRule for type=%d is rule=%16p\n", vm->compiler->parser->previous.type, prefixRule);
  if (prefixRule == NULL) {
    error(vm->compiler->parser, "Expect expression.");
    return;
  }

  bool canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(vm, canAssign);

  while (precedence <= getRule(vm->compiler->parser->current.type)->precedence) {
    advance(vm->compiler->parser);
    ParseFn infixRule = getRule(vm->compiler->parser->previous.type)->infix;
    infixRule(vm, canAssign);
  }

  if (canAssign && match(vm->compiler->parser, TOKEN_EQUAL)) {
    error(vm->compiler->parser, "Invalid assignment target.");
  }
}

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

// An EXPRESSION produces a single VALUE on the stack
static void expression(VM* vm) {
  parsePrecedence(vm, PREC_ASSIGNMENT);
}

static void block(VM* vm) {
  while (!check(vm->compiler->parser, TOKEN_RIGHT_BRACE) && !check(vm->compiler->parser, TOKEN_EOF)) {
    declaration(vm);
  }

  consume(vm->compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(VM* vm, FunctionType type) {
  Compiler compiler;
  initCompiler(vm, &compiler, vm->compiler->parser, type);
  beginScope(vm);

  // Compile the parameter list.
  consume(vm->compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
  if (!check(vm->compiler->parser, TOKEN_RIGHT_PAREN)) {
    do {
//      current->function->arity++;
      vm->compiler->function->arity++;
//      if (current->function->arity > 255) {
      if (vm->compiler->function->arity > 255) {
        errorAtCurrent(vm->compiler->parser, "Cannot have more than 255 parameters.");
      }

//      uint8_t paramConstant = parseVariable(vm, "Expect parameter name.");
      uint16_t paramConstant = parseVariable(vm, "Expect parameter name.");
      defineVariable(vm, paramConstant);
    } while (match(vm->compiler->parser, TOKEN_COMMA));
  }
  consume(vm->compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

  // The body.
  consume(vm->compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block(vm);

  // Create the function object.
  ObjFunction* function = endCompiler(vm);
  //printf("EXPERIMENTAL FIX FOR GC BUG: push()\n");
  push(vm, OBJ_VAL(function)); // EXPERIMENTAL FIX FOR GC BUG
  emitByte(vm, OP_CLOSURE);
  uint16_t constant = makeConstant(vm, OBJ_VAL(function));
  //printf("EXPERIMENTAL FIX FOR GC BUG: pop()\n");
  pop(vm); // EXPERIMENTAL FIX FOR GC BUG

  emitWord(vm, constant);

  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(vm, compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(vm, compiler.upvalues[i].index);
  }
}

static void method(VM* vm) {
  //printf("compiler:method() called\n");
  consume(vm->compiler->parser, TOKEN_IDENTIFIER, "Expect method name.");
  uint16_t constant = identifierConstant(vm, &vm->compiler->parser->previous);

  FunctionType type = TYPE_METHOD;
  //printf ("compiler:method() name is %.*s\n", vm->compiler->parser->previous.length, vm->compiler->parser->previous.start);
  if (vm->compiler->parser->previous.length == 4 && memcmp(vm->compiler->parser->previous.start, "init", 4) == 0) {
    //printf("compiler:method() type is TYPE_INITIALIZER\n");
    type = TYPE_INITIALIZER;
  }
  function(vm, type);
  emitByte(vm, OP_METHOD);
  emitWord(vm, constant);
}

static void classDeclaration(VM* vm) {
  consume(vm->compiler->parser, TOKEN_IDENTIFIER, "Expect class name.");
//  uint8_t nameConstant = identifierConstant(vm, &vm->compiler->parser->previous);
  Token* className = &vm->compiler->parser->previous; // Note the name
  uint16_t nameConstant = identifierConstant(vm, &vm->compiler->parser->previous);
  declareVariable(vm);

//  emitBytes(vm, OP_CLASS, nameConstant);
  emitByte(vm, OP_CLASS);
  emitWord(vm, nameConstant);
  defineVariable(vm, nameConstant);

  ClassCompiler classCompiler;
  //classCompiler.name = parser.previous;
  //classCompiler.enclosing = currentClass;
  //currentClass = &classCompiler;
  classCompiler.name = vm->compiler->parser->previous; // name is a Token
  classCompiler.hasSuperclass = false;
  classCompiler.enclosing = vm->currentClass;
  vm->currentClass = &classCompiler;

  // : Superclass
  if (match(vm->compiler->parser, TOKEN_COLON)) {
    consume(vm->compiler->parser, TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(vm, false); // Look up superclass by name, push it on the stack

    if (identifiersEqual(&className, &vm->compiler->parser->previous)) {
      error(vm->compiler->parser, "A class cannot inherit from itself.");
    }

    beginScope(vm);
    addLocal(vm, syntheticToken("super"));
    defineVariable(vm, 0);

    namedVariable(vm, *className, false); // Load this class onto the stack
    emitByte(vm, OP_INHERIT);
    classCompiler.hasSuperclass = true;
  }

  namedVariable(vm, *className, false); // Put the class name on the stack
  consume(vm->compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before class body.");
  while (!check(vm->compiler->parser, TOKEN_RIGHT_BRACE) && !check(vm->compiler->parser, TOKEN_EOF)) {
    // We don't have field declarations so anything here must be a method
    // Hmm.
    method(vm);
  }
  consume(vm->compiler->parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  emitByte(vm, OP_POP); // We no longer need the class name

  if (classCompiler.hasSuperclass) {
    endScope(vm);
  }
  //currentClass = currentClass->enclosing;
  vm->currentClass = vm->currentClass->enclosing; // Nor its compiler struct
}

static void funDeclaration(VM* vm) {
//  uint8_t global = parseVariable(vm, "Expect function name.");
  uint16_t global = parseVariable(vm, "Expect function name.");
  markInitialized(vm);
  function(vm, TYPE_FUNCTION);
  defineVariable(vm, global);
}

static void varDeclaration(VM* vm) {
//  uint8_t global = parseVariable(vm, "Expect variable name.");
  uint16_t global = parseVariable(vm, "Expect variable name.");

  if (match(vm->compiler->parser, TOKEN_EQUAL)) {
    expression(vm);
  } else {
    emitByte(vm, OP_NULL);
  }
  consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  defineVariable(vm, global);
}

static void breakStatement(VM* vm) {
  if (innermostBreakScopeStart == -1) {
    error(vm->compiler->parser, "Cannot use 'break' outside of a loop or switch.");
  }
  consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

  // Discard any locals created inside the loop.
//  for (int i = current->localCount - 1;
  for (int i = vm->compiler->localCount - 1;
//       i >= 0 && current->locals[i].depth > innermostBreakScopeDepth;
       i >= 0 && vm->compiler->locals[i].depth > innermostBreakScopeDepth;
       i--) {
    emitByte(vm, OP_POP);
  }

  innermostBreakJump[innermostBreakJumps++] = emitJump(vm, OP_JUMP);
}

static void continueStatement(VM* vm) {
  if (innermostLoopStart == -1) {
    error(vm->compiler->parser, "Cannot use 'continue' outside of a loop.");
  }
  consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

  // Discard any locals created inside the loop.
//  for (int i = current->localCount - 1;
//       i >= 0 && current->locals[i].depth > innermostLoopScopeDepth;
  for (int i = vm->compiler->localCount - 1;
       i >= 0 && vm->compiler->locals[i].depth > innermostLoopScopeDepth;
       i--) {
    emitByte(vm, OP_POP);
  }

  // Jump to top of current innermost loop.
  emitLoop(vm, innermostLoopStart);
}

static void exitStatement(VM* vm) {
  //printf("compiler:exitStatement() begin\n");
  emitByte(vm, OP_EXIT);
  consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after exit.");
  //printf("compiler:exitStatement() done\n");
}


static void expressionStatement(VM* vm) {
  expression(vm);
  consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after expression.(#exprStmt)");
  emitByte(vm, OP_POP);
}

static void forStatement(VM* vm) {
  beginScope(vm);
  consume(vm->compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

  // Initializer?
  if (match(vm->compiler->parser, TOKEN_SEMICOLON)) {
    // No initializer.
  } else if (match(vm->compiler->parser, TOKEN_VAR)) {
    varDeclaration(vm); // E.g. "var i=0"
  } else {
    expressionStatement(vm); // E.g. "i=0"
  }

  // Note the loop starting point
  // both for "continue" and for the loop itself
  int surroundingLoopStart = innermostLoopStart;
  int surroundingLoopScopeDepth = innermostLoopScopeDepth;
  innermostLoopStart = currentChunk(vm)->count;
//  innermostLoopScopeDepth = current->scopeDepth;
  innermostLoopScopeDepth = vm->compiler->scopeDepth;

  // for "break"
  int surroundingBreakScopeStart = innermostBreakScopeStart;
  int surroundingBreakScopeDepth = innermostBreakScopeDepth;
  int* surroundingBreakJump = innermostBreakJump;
  int surroundingBreakJumps = innermostBreakJumps;
  innermostBreakScopeStart = currentChunk(vm)->count;
//  innermostBreakScopeDepth = current->scopeDepth;
  innermostBreakScopeDepth = vm->compiler->scopeDepth;
  innermostBreakJump = malloc(MAX_BREAKS_PER_SCOPE * sizeof(int));
  //printf("ifStatement() allocated bpjump buffer %p\n", innermostBreakJump);
  innermostBreakJumps = 0;

  // Condition?
  int exitJump = -1;
  if (!match(vm->compiler->parser, TOKEN_SEMICOLON)) {
    expression(vm);
    consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    exitJump = emitJump(vm, OP_PJMP_IF_FALSE);
    //emitByte(OP_POP); // Condition.
  }

  // Increment?
  if (!match(vm->compiler->parser, TOKEN_RIGHT_PAREN)) {
    // The increment runs _after_ the loop body
    // so we need to jump back and forth a little.
    int bodyJump = emitJump(vm, OP_JUMP); // Jump to just before the loop body

    int incrementStart = currentChunk(vm)->count;
    expression(vm);
    emitByte(vm, OP_POP);
    consume(vm->compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

    //emitLoop(loopStart); // Jump to just before the loop condition
    //loopStart = incrementStart;
    emitLoop(vm, innermostLoopStart); // <-- for "continue"
    innermostLoopStart = incrementStart; // <-- for "continue"

    patchJump(vm, bodyJump);
  }

  // The loop body starts here
  statement(vm);

  //emitLoop(loopStart); // Jump to incrementStart (if any), otherwise loopStart
  emitLoop(vm, innermostLoopStart); // <--

  // Mark the exit point for the loop
  if (exitJump != -1) {
    patchJump(vm, exitJump);
    //emitByte(OP_POP); // Condition.
  }

  innermostLoopStart = surroundingLoopStart; // <-- for "continue"
  innermostLoopScopeDepth = surroundingLoopScopeDepth; // <-- for "continue"

  for (int i = 0; i < innermostBreakJumps; i++) {
    //printf("ifStatement() patching break jump %d\n", i);
    patchJump(vm, innermostBreakJump[i]);
  }

  // for "break"
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
  printf("compiler:forStatement() free(%p) // innermostBreakJump\n", (void*) innermostBreakJump);
#endif
  free(innermostBreakJump);

  //printf("ifStatement() freed bpjump buffer %p\n", innermostBreakJump);
  innermostBreakScopeStart = surroundingBreakScopeStart;
  innermostBreakScopeDepth = surroundingBreakScopeDepth;
  innermostBreakJump = surroundingBreakJump;
  innermostBreakJumps = surroundingBreakJumps;

  endScope(vm);
}

static void ifStatement(VM* vm) {
  consume(vm->compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
  expression(vm);
  consume(vm->compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int thenJump = emitJump(vm, OP_PJMP_IF_FALSE);
  //emitByte(OP_POP); // We no longer need the jump condition
  statement(vm);

  int elseJump = emitJump(vm, OP_JUMP); // Mark end of THEN block

  patchJump(vm, thenJump);
  //emitByte(OP_POP); // We no longer need the jump condition

  if (match(vm->compiler->parser, TOKEN_ELSE)) statement(vm);
  patchJump(vm, elseJump);
}

static void printStatement(VM* vm) {
  expression(vm);
  consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after value.");
  emitByte(vm, OP_PRINT);
}

static void returnStatement(VM* vm) {
//  if (current->type == TYPE_SCRIPT) {
  if (vm->compiler->type == TYPE_SCRIPT) {
    error(vm->compiler->parser, "Cannot return from top-level code.");
  }

  if (match(vm->compiler->parser, TOKEN_SEMICOLON)) {
    emitReturn(vm); // No return value specified
  } else {
    //if (current->type == TYPE_INITIALIZER) {
    if (vm->compiler->type == TYPE_INITIALIZER) {
      error(vm->compiler->parser, "Cannot return a value from an initializer.");
    }
    expression(vm);
    consume(vm->compiler->parser, TOKEN_SEMICOLON, "Expect ';' after return value.");
    emitByte(vm, OP_RETURN);
  }
}



static void switchStatement(VM* vm) {
  beginScope(vm);

  consume(vm->compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
  expression(vm);
  consume(vm->compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after value.");
  consume(vm->compiler->parser, TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");

  int state = 0; // 0: before all cases, 1: before default, 2: after default.

  // for "break"
  int surroundingBreakScopeStart = innermostBreakScopeStart;
  int surroundingBreakScopeDepth = innermostBreakScopeDepth;
  int* surroundingBreakJump = innermostBreakJump;
  int surroundingBreakJumps = innermostBreakJumps;
  innermostBreakScopeStart = currentChunk(vm)->count;
//  innermostBreakScopeDepth = current->scopeDepth;
  innermostBreakScopeDepth = vm->compiler->scopeDepth;
  innermostBreakJump = malloc(MAX_BREAKS_PER_SCOPE * sizeof(int));
  //printf("switchStatement() allocated bpjump buffer %p\n", innermostBreakJump);
  innermostBreakJumps = 0;

  int previousCaseSkip = -1;
  int fallThrough = -1;

  while (!match(vm->compiler->parser, TOKEN_RIGHT_BRACE) && !check(vm->compiler->parser, TOKEN_EOF)) {

    if (match(vm->compiler->parser, TOKEN_CASE) || match(vm->compiler->parser, TOKEN_DEFAULT)) {
      TokenType caseType = vm->compiler->parser->previous.type;

      if (state == 2) {
        error(vm->compiler->parser, "Cannot have cases after the default case.");
      }

      if (state == 1) {
        // If control reashes this point, it means the previous case matched
        // but did not break. This means we need a fallthrough jump.
        fallThrough = emitJump(vm, OP_JUMP);

        // If that case didn't match, patch its condition to jump to jump here.
        patchJump(vm, previousCaseSkip);
      }

      if (caseType == TOKEN_CASE) {
        state = 1;
        // Case, evaluate expression then compare
        emitByte(vm, OP_DUP); // The comparison will pop the Value so duplicate it
        expression(vm);

        consume(vm->compiler->parser, TOKEN_COLON, "Expect ':' after case value.");

        emitByte(vm, OP_EQUAL);
        previousCaseSkip = emitJump(vm, OP_PJMP_IF_FALSE);

      } else {
        // Default
        state = 2;
        consume(vm->compiler->parser, TOKEN_COLON, "Expect ':' after default.");
        previousCaseSkip = -1;
      }

      // Both CASE and DEFAULT must patch fallThrough if there is one
      if (fallThrough != -1) {
        patchJump(vm, fallThrough);
        fallThrough = -1;
      }

    } else {
      // Not CASE or DEFAULT so this is a statement inside the current case.
      if (state == 0) {
        error(vm->compiler->parser, "Cannot have statements before any case.");
      }
      statement(vm);
    }
  }

  // If we ended without a default case, patch its condition jump.
  if (state == 1) {
    if (previousCaseSkip != -1) {
      patchJump(vm, previousCaseSkip);
      previousCaseSkip = -1;
    }
  }

  // Patch all break jumps
  for (int i = 0; i < innermostBreakJumps; i++) {
    patchJump(vm, innermostBreakJump[i]);
  }

  // for "break"
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
  printf("compiler:switchStatement() free(%p) // innermostBreakJump\n", (void*) innermostBreakJump);
#endif
  free(innermostBreakJump);
  //printf("switchStatement() freed bpjump buffer %p\n", innermostBreakJump);
  innermostBreakScopeStart = surroundingBreakScopeStart;
  innermostBreakScopeDepth = surroundingBreakScopeDepth;
  innermostBreakJump = surroundingBreakJump;
  innermostBreakJumps = surroundingBreakJumps;

  emitByte(vm, OP_POP); // The switch value.
  endScope(vm);
}


static void whileStatement(VM* vm) {

  // Note the loop starting point
  // both for "continue" and for the loop itself
  int surroundingLoopStart = innermostLoopStart;
  int surroundingLoopScopeDepth = innermostLoopScopeDepth;
  innermostLoopStart = currentChunk(vm)->count;
//  innermostLoopScopeDepth = current->scopeDepth;
  innermostLoopScopeDepth = vm->compiler->scopeDepth;

  // for "break"
  int surroundingBreakScopeStart = innermostBreakScopeStart;
  int surroundingBreakScopeDepth = innermostBreakScopeDepth;
  int* surroundingBreakJump = innermostBreakJump;
  int surroundingBreakJumps = innermostBreakJumps;
  innermostBreakScopeStart = currentChunk(vm)->count;
//  innermostBreakScopeDepth = current->scopeDepth;
  innermostBreakScopeDepth = vm->compiler->scopeDepth;
  innermostBreakJump = malloc(MAX_BREAKS_PER_SCOPE * sizeof(int));
  //printf("whileStatement() allocated bpjump buffer %p\n", innermostBreakJump);
  innermostBreakJumps = 0;

  consume(vm->compiler->parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
  expression(vm);
  consume(vm->compiler->parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

  int exitJump = emitJump(vm, OP_PJMP_IF_FALSE);

  statement(vm);
  emitLoop(vm, innermostLoopStart); // <-- for "continue"


  patchJump(vm, exitJump);
  innermostLoopStart = surroundingLoopStart; // <-- for "continue"
  innermostLoopScopeDepth = surroundingLoopScopeDepth; // <-- for "continue"

  for (int i = 0; i < innermostBreakJumps; i++) {
    //printf("whileStatement() patching break jump %d\n", i);
    patchJump(vm, innermostBreakJump[i]);
  }

  // for "break"
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
  printf("compiler:whileStatement() free(%p) // innermostBreakJump\n", (void*) innermostBreakJump);
#endif
  free(innermostBreakJump);
  //printf("whileStatement() freed bpjump buffer %p\n", innermostBreakJump);
  innermostBreakScopeStart = surroundingBreakScopeStart;
  innermostBreakScopeDepth = surroundingBreakScopeDepth;
  innermostBreakJump = surroundingBreakJump;
  innermostBreakJumps = surroundingBreakJumps;

}

// Exit "panic mode" caused by an unexpected token
// Skip tokens indiscriminately until we reach a semicolon or a statement token.
static void synchronize(VM* vm) {
  vm->compiler->parser->panicMode = false;

  while (vm->compiler->parser->current.type != TOKEN_EOF) {
    if (vm->compiler->parser->previous.type == TOKEN_SEMICOLON) return;

    switch (vm->compiler->parser->current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
        // Do nothing.
        ;
    }

    advance(vm->compiler->parser);
  }
}


// A DECLARATION is a STATEMENT that assigns a VALUE to an IDENTIFIER
static void declaration(VM* vm) {
  if (match(vm->compiler->parser, TOKEN_CLASS)) {
    classDeclaration(vm);
  } else if (match(vm->compiler->parser, TOKEN_FUN)) {
    funDeclaration(vm);
  } else if (match(vm->compiler->parser, TOKEN_VAR)) {
    varDeclaration(vm);
  } else {
    statement(vm);
  }

  if (vm->compiler->parser->panicMode) synchronize(vm);
}

// A STATEMENT assigns a VALUE to or invokes an IDENTIFIER
static void statement(VM* vm) {
  if (match(vm->compiler->parser, TOKEN_PRINT)) {
    printStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_BREAK)) {
    breakStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_CONTINUE)) {
    continueStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_EXIT)) {
    exitStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_FOR)) {
    forStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_IF)) {
    ifStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_RETURN)) {
    returnStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_SWITCH)) {
    switchStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_WHILE)) {
    whileStatement(vm);
  } else if (match(vm->compiler->parser, TOKEN_LEFT_BRACE)) {
    beginScope(vm);
    block(vm);
    endScope(vm);
  } else {
    expressionStatement(vm);
  }
}


// Main entry point
ObjFunction* compile(void* vm, int fileno, const char* source) {
#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:compile() vm=%p fileno=%d source length=%d\n", vm, fileno, (int)strlen(source));
#endif
  Compiler compiler;

  Parser* parser = initParser(vm, fileno, source);
#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:compile() initialized parser %p\n", parser);
#endif

  initCompiler(vm, &compiler, parser, TYPE_SCRIPT);
#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:compile() initialized compiler %p\n", &compiler);
#endif

  advance(((VM*)vm)->compiler->parser);

  while (!match(((VM*)vm)->compiler->parser, TOKEN_EOF)) {
    declaration(vm);
  }

  ObjFunction* function = endCompiler(vm);


#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:compile() checking status:\n");
  printf("compiler:compile()   vm %p\n", vm);
  printf("compiler:compile()   compiler %p\n", &compiler);
  printf("compiler:compile()   parser %p\n", compiler.parser);
  printf("compiler:compile()   error %d\n", (int)compiler.parser->hadError );
#endif
  //if (((VM*)vm)->compiler->parser->hadError) function = NULL;
  if (compiler.parser->hadError) function = NULL;

  destroyParser(vm, parser);

#ifdef DEBUG_TRACE_COMPILER
  printf("compiler:compile() done, returning function %p\n", function);
#endif
  return function;
}

// Garbage collect compiler Objs
void markCompilerRoots(void* vm) {
//  Compiler* compiler = current;
  Compiler* compiler = ((VM*)vm)->compiler;
#ifdef DEBUG_LOG_GC_VERBOSE
  printf("compiler:markCompilerRoots() vm=%p compiler=%p\n", vm, compiler);
#endif
  while (compiler != NULL) {
    markObject((VM*)vm, (Obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}

