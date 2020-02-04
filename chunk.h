#ifndef clox_chunk_h
#define clox_chunk_h


#include "common.h"
#include "value.h"

//struct VM;

typedef enum {
  OP_CONSTANT,      // push constant
  OP_NULL,          // push null
  OP_TRUE,          // push true
  OP_FALSE,         // push false
  OP_POP,           // pop a, discard
  OP_POPN,          // pop N values, discard
  OP_DUP,           // push(peek top of stack)
  OP_GET_LOCAL,     // get index bytecode, peek stack[index] for value, then push a copy
  OP_SET_LOCAL,     // get index bytecode, peek top of stack, poke the value into stack[index]
  OP_GET_GLOBAL,    // get constant bytecode, get value from hash, then push
  OP_DEFINE_GLOBAL, // get constant bytecode, store value in hash, then pop
  OP_SET_GLOBAL,    // get constant bytecode, store value
  OP_MAKE_ARRAY,    // get length, make array of stack values, pop values, push array value
  OP_GET_INDEX,     // EXPERIMENTAL
  OP_SET_INDEX,     // EXPERIMENTAL
  OP_GET_SLICE,     // EXPERIMENTAL
  OP_SET_SLICE,     // EXPERIMENTAL
  OP_GET_UPVALUE,   // a closure thing. (comment tbd if/when I get it.)
  OP_SET_UPVALUE,   // a closure thing. (comment tbd if/when I get it.)
  OP_GET_PROPERTY,  // get property of class instance
  OP_SET_PROPERTY,  // set property (field) of class instance
  OP_EQUAL,         // pop b, pop a, push bool(a==b)
  OP_NEQUAL,        // pop b, pop a, push bool(a!=b)
  OP_GREATER,       // pop b, pop a, push bool(a>b)
  OP_GEQUAL,        // pop b, pop a, push bool(a>=b)
  OP_INC,           // pop a, push a+1
  OP_DEC,           // pop a, push a-1
  OP_LESS,          // pop b, pop a, push bool(a<b)
  OP_LEQUAL,        // pop b, pop a, push bool(a<=b)
  OP_ADD,           // pop b, pop a, push a+b
  OP_SUBTRACT,      // pop b, pop a, push a-b
  OP_MULTIPLY,      // pop b, pop a, push a*b
  OP_DIVIDE,        // pop b, pop a, push a/b
  OP_NOT,           // pop a, push !a
  OP_NEGATE,        // pop a, push -a
  OP_BIN_NOT,       // pop a, push ~a             NOTE: uint32_t
  OP_BIN_SHIFTL,    // pop a, pop b, push(a << b) NOTE: uint32_t
  OP_BIN_SHIFTR,    // pop a, pop b, push(a >> b) NOTE: uint32_t
  OP_BIN_AND,       // pop a, pop b, push(a & b)  NOTE: uint32_t
  OP_BIN_OR,        // pop a, pop b, push(a | b)  NOTE: uint32_t
  OP_BIN_XOR,       // pop a, pop b, push(a ^ b)  NOTE: uint32_t
  OP_PRINT,         // pop a, print a
  OP_JUMP,          // pop hi, pop lo, ip += (hi<<8)|lo
  OP_PJMP_IF_FALSE, // pop hi, pop lo, POP a, IF a==falsey THEN ip += (hi<<8)|lo
  OP_QJMP_IF_FALSE, // pop hi, pop lo, PEEK a, IF a==falsey THEN ip += (hi<<8)|lo
  OP_LOOP,          // pop hi, pop lo, ip -= (hi<<8)|lo
  OP_CALL,          // call a function. (sounds easy. isn't.)
  OP_CLOSURE,       // a closure thing. (tbd if/when I get it.)
  OP_CLOSE_UPVALUE, // another, even stranger closure thing
  OP_RETURN,        //
  OP_CLASS,         //
} OpCode;

typedef struct {
  int         count;       // Elements currently stored
  int         capacity;    // Total element capacity
  uint8_t*    code;        // OpCode
  int*        files;       // Source code file number
  int*        lines;       // Source code line number
  int*        chars;       // Source code char number
  ValueArray  constants;   // Literal values (value.h)
} Chunk;

void initChunk(void* vm, Chunk* chunk);
void freeChunk(void* vm, Chunk* chunk);
void writeChunk(void* vm, Chunk* chunk, uint8_t byte, int fileno, int lineno, int charno);
int addConstant(void* vm, Chunk* chunk, Value value);
void writeConstant(void* vm, Chunk* chunk, Value value, int fileno, int lineno, int charno);

#endif
