#ifndef cfun_vm_h
#define cfun_vm_h

//#include "chunk.h"
#include "compiler.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)



typedef struct {
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame; // 1 call frame = 1 ongoing function call


typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILED,
  INTERPRET_RUNNING,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

struct Parser;
struct Compiler;

typedef struct FunVM {
  CallFrame frames[FRAMES_MAX];
  int frameCount;

  Value stack[STACK_MAX];
  Value* stackTop;
  Table globals; // Global variables
  Table strings; // Internalized, unique strings
  ObjString* initString; // Literally "init", used for calling object initializers
  ObjUpvalue* openUpvalues; // Umm. Yea. Those.

  size_t bytesAllocated;
  size_t nextGC;

  double sleep;
  bool yield;
  ErrorCb error_callback;
  char* errbuf; // For compiler errors -- replace with callback FIXME

  Obj* objects;  // Code chunks. Variables. Constants.

  struct Parser* parser;
  struct Compiler* compiler; // current
  struct ClassCompiler* currentClass;

  int grayCount; // GC graystack slots in use
  int grayCapacity; // GC graystack slot capacity
  Obj** grayStack; // Pointer to GC graystack bottom
} VM;


// API functions
Value to_numberValue(double n);
Value to_stringValue(VM* vm, const char* cstr);
double to_double(Value v);
char* to_cstring(Value v);
bool is_number(Value v);
bool is_string(Value v);

VM* initVM();
void freeVM(VM* vm);
void defineNative(VM* vm, const char* name, NativeFn function);
void set_error_callback(VM* vm, ErrorCb ptr);
void runtimeError(VM* vm, const char* format, ...);
InterpretResult run(VM* vm);
InterpretResult interpret(VM* vm, const char* source);
void push(VM* vm, Value value);
Value pop(VM* vm);





#endif

