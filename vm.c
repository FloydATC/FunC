//#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//#include <sys\timeb.h>
#include <math.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "error.h"
#include "file.h"
#include "index.h"
#include "vm.h"
#include "object.h"
#include "objarray.h"
#include "objstring.h"
#include "memory.h"

//VM vm;
// The choice to have a static VM instance is a concession for the book,
// but not necessarily a sound engineering choice for a real
// language implementation. If you’re building a VM that’s designed to be
// embedded in other host applications, it gives the host more flexibility
// if you do explicitly take a VM pointer and pass it around.


static void resetStack(VM* vm) {
  vm->stackTop = vm->stack;
  vm->frameCount = 0;
  vm->openUpvalues = NULL;
}



void runtimeError(VM* vm, const char* format, ...) {
  char* msgbuf = NULL;

  va_list args;
  va_start(args, format);
  //vfprintf(stderr, format, args);
  msgbuf = vbprintf(msgbuf, format, args);
  va_end(args);
  //fputs("\n", stderr);
  msgbuf = bprintf(msgbuf, "\n");

  // Print stack trace
  for (int i = vm->frameCount - 1; i >= 0; i--) {
  //for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm->frames[i];
    ObjFunction* function = frame->closure->function;
    // -1 because the IP is sitting on the next instruction to be
    // executed.
    size_t ip = frame->ip - function->chunk.code - 1;
    int line = function->chunk.lines[ip];
    if (function->name == NULL) {
      msgbuf = bprintf(msgbuf, "line %d\n", line);
    } else {
      char* fn = function->name->chars;
      msgbuf = bprintf(msgbuf, "line %d in %s\n", line, fn);
    }

  }

  // Print buffer on stderr
  fprintf(stderr, msgbuf);

  // Callback with buffer
  if (vm->error_callback != NULL) { vm->error_callback(msgbuf); }

  // Deallocate error message buffer
  printf("vm:runtimeError() free(%p) // message buffer\n", (void*) msgbuf);
  free(msgbuf);

  resetStack(vm);
}



// Helper functions for the API

Value to_numberValue(double n) {
  return NUMBER_VAL(n);
}

Value to_stringValue(VM* vm, const char* cstr) {
  // We do not own cstr so we must copy it
  //printf("vm:to_stringValue() must import %p\n", (void*)cstr);
  int length = (int) strlen(cstr);
  //printf("vm:to_stringValue() copyString %p (%d bytes)\n", (void*)cstr, length);
  ObjString* result = copyString(vm, cstr, length);
  //printf("vm:to_stringValue() ObjString* = %p\n", (void*)result );
  Value v = OBJ_VAL(result);
  //printf("vm:to_stringValue() returning Value with ObjString %p\n", (void*)v.as.obj);
  return v;
}

double to_double(Value v) {
  if (IS_NUMBER(v)==true) { return AS_NUMBER(v); }
  if (IS_STRING(v)==true) { return strtod(AS_CSTRING(v), NULL); }
  return 0;
}

char* to_cstring(Value v) {
  //printf("to_cstring() called\n");
  if (IS_STRING(v)==true) {
    char* s = AS_CSTRING(v);
    //printf("to_cstring() returning string as %p\n", s);
    return s;
  }
  if (IS_NUMBER(v)==true) {
    static char x[1024];
    sprintf(x, "%g", AS_NUMBER(v));
    //printf("to_cstring() returning number as %s\n", x);
    return x;
  }
  return "";
}

bool is_number(Value v) {
  return IS_NUMBER(v);
}

bool is_string(Value v) {
  return IS_STRING(v);
}

double now() {
  //struct timeb t;
  //ftime(&t);
  //double ts = t.time + (t.millitm / 1000.0);
  //printf("now() = %f\n", ts);
  //return ts;
  long            ms; // Milliseconds
  time_t          s;  // Seconds
  struct timespec spec;
  double ts;
  clock_gettime(CLOCK_REALTIME, &spec);

  s  = spec.tv_sec;
  ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
  if (ms > 999) {
    s++;
    ms = 0;
  }
  ts = s + (ms/1000.0);
  printf("now() = %f\n", ts);
  return ts;
}



// Native C function: clock()
static bool clockNative(void* vm, int argCount, Value* args, Value* result) {
  (unused)vm;
  (unused)argCount;
  (unused)args;
  *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
  return true;
}

// Native C function: sleep()
static bool sleepNative(void* vm, int argCount, Value* args, Value* result) {
  (unused)argCount;
  // We can't just call sleep, that would block everything.
  // Instead, set vm.sleep and let the VM take care of it.
  // Argument is in milliseconds, 0 will just yield
  ((VM*)vm)->sleep = now() + (AS_NUMBER(args[0]) / 1000.0);
  ((VM*)vm)->yield = true;
  *result = args[0];
  return true;
}


// Native C function: c_test()
static bool c_test(void* vm, int argCount, Value* args, Value* result) {
  (unused)vm;
  printf("c_test() argCount=%d\n", argCount);
  printf("c_test() args pointer=%p\n", args);
  for (int i=0; i<argCount; i++) {
    printf("c_test() arg%d=%g\n", i, AS_NUMBER(args[i]));
  }
  *result = NUMBER_VAL((double) 234);
  return true;
}



void set_error_callback(VM* vm, ErrorCb ptr) {
  vm->error_callback = ptr;
}



void defineNative(VM* vm, const char* name, NativeFn function) {
  push(vm, OBJ_VAL(copyString(vm, name, (int)strlen(name))));
  push(vm, OBJ_VAL(newNative(vm, function)));
  tableSet(vm, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
  pop(vm);
  pop(vm);
}

void freeVM(VM* vm) {
  if (vm->errbuf != NULL) {
    //printf("vm.freeVM(%p) freeing error buffer %p\n", (void*)vm, (void*)vm->errbuf);
    free(vm->errbuf);
    vm->errbuf = NULL;
  }
  //printf("vm.freeVM(%p) freeing globals\n", (void*)vm);
  freeTable(vm, &vm->globals);
  //printf("vm.freeVM(%p) freeing strings\n", (void*)vm);
  freeTable(vm, &vm->strings);
  //printf("vm.freeVM(%p) freeing objects\n", (void*)vm);
  freeObjects(vm);
  //printf("vm.freeVM(%p) freeing struct\n", (void*)vm);
  free(vm);
  vm = NULL;
  //printf("vm.freeVM() done\n");
}

void push(VM* vm, Value value) {
  *vm->stackTop = value;
  vm->stackTop++;
}

Value pop(VM* vm) {
  vm->stackTop--;
  return *vm->stackTop;
}

static Value peek(VM* vm, int distance) {
  return vm->stackTop[-1 - distance];
}

static bool call(VM* vm, ObjClosure* closure, int argCount) {
  if (argCount != closure->function->arity) {
    runtimeError(vm, "Expected %d arguments but got %d.",
        closure->function->arity, argCount);
    return false;
  }

  if (vm->frameCount == FRAMES_MAX) {
    runtimeError(vm, "Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm->frames[vm->frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;

  frame->slots = vm->stackTop - argCount - 1;
  return true;
}



// Native C method: NUMBER.base()
static bool number_base(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1) {
    runtimeError(vm, "Method needs one argument.");
    return false;
  }
  if (!IS_NUMBER(args[0])) {
    runtimeError(vm, "Argument must be a number.");
    return false;
  }
  int base = AS_NUMBER(args[0]);
  char* chars = "0123456789abcdef";
  uint64_t input = (uint64_t) AS_NUMBER(receiver);
  char string[65];
  int length = 0;
  if (base < 2 || base > 16) {
    runtimeError(vm, "Base must be 2-16.");
    return false;
  }

  for (int i=64; i>0; i--) {
    double m = pow(base, i-1);
    int n = floor(input / m);
    if (n>0 || length>0 || i==1) { // Skip leading 0 unless it is the only digit
      string[length++] = chars[n];
    }
    input -= m * n;
  }
  string[length] = '\0';

  *result = OBJ_VAL(copyString(vm, string, length));
  return true;
}

static bool callValue(VM* vm, Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm->stackTop[-argCount - 1] = OBJ_VAL(newInstance(vm, klass));
        return true;
      }
      case OBJ_CLOSURE: {
        return call(vm, AS_CLOSURE(callee), argCount);
      }
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        //printf("vm:callValue() will call %p with argCount=%d, args=%p\n", native, argCount,(vm->stackTop - argCount));
        for (int i=0; i<argCount; i++) {
          //printf("vm:callValue() checking arg %d\n", i);
          if (!IS_NUMBER(peek(vm, i)) && !IS_STRING(peek(vm, i))) {
            runtimeError(vm, "Expected string or number argument.");
            return false;
          }
        }
        Value result;
        //printf("vm:callValue() args ok, calling\n");
        bool success = native(vm, argCount, vm->stackTop - argCount, &result);
        //printf("vm:callValue() returned from native function\n");
        vm->stackTop -= argCount + 1; // Discard arguments from stack
        //printf("vm:callValue() pushing result to stack\n");
        push(vm, result);
        if (!success) {
          runtimeError(vm, "Call failed, check arguments.");
          return false;
        }
        return true;
      }
      case OBJ_NATIVEMETHOD: {
        ObjNativeMethod* method = AS_NATIVEMETHOD(callee);
        //printf("vm:callValue() will call %p with argCount=%d, args=%p\n", native, argCount,(vm->stackTop - argCount));
        Value result;
        //printf("vm:callValue() args ok, calling\n");
        bool success = method->function(vm, method->receiver, argCount, vm->stackTop - argCount, &result);
        //printf("vm:callValue() returned from native function\n");
        vm->stackTop -= argCount + 1; // Discard arguments from stack
        //printf("vm:callValue() pushing result to stack\n");
        push(vm, result);
        if (!success) {
          runtimeError(vm, "Call failed, check arguments.");
          return false;
        }
        return true;
      }
      default:
        // Non-callable object type.
        break;
    }
  }

  runtimeError(vm, "Can only call functions and classes.");
  return false;
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm->openUpvalues;

  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) return upvalue;

  ObjUpvalue* createdUpvalue = newUpvalue(vm, local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm->openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(VM* vm, Value* last) {
  while (vm->openUpvalues != NULL &&
         vm->openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm->openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->openUpvalues = upvalue->next;
  }
}

static bool isFalsey(Value value) {
  return IS_NULL(value) || // NULL is falsey
        (IS_BOOL(value) && !AS_BOOL(value)) || // FALSE is falsey
        (IS_NUMBER(value) && AS_NUMBER(value) == 0) || // The number 0 is falsey
        (IS_STRING(value) && ((ObjString*)AS_OBJ(value))->length == 0) || // Empty string "" is falsey
        (IS_ARRAY(value) && ((ObjArray*)AS_OBJ(value))->length == 0); // Empty array [] is falsey
}


// String + String = String
static void concatenateStrings(VM* vm) {
  // takeString() may trigger GC so peek() instead of pop()
  ObjString* b = AS_STRING(peek(vm, 0));
  ObjString* a = AS_STRING(peek(vm, 1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(vm, char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(vm, chars, length);
  // Now "a" and "b" can be popped safely
  pop(vm);
  pop(vm);
  push(vm, OBJ_VAL(result));
}


// Pop the specified number of values, push an Array containing those values
static void makeArray(VM* vm, uint8_t length) {
  //printf("vm:makeArray() calling newArray()\n");
  // Create ObjArray and immediately push it onto the stack
  ObjArray* object = newArray(vm);
  Value array = OBJ_VAL(object);
  push(vm, array); // Store temporarily
  //printf("vm:makeArray() array %p pushed onto stack\n", object);
  loadArray(vm, object, vm->stackTop - length - 1, length);
  //printf("vm:makeArray() array %p construction complete\n", object);
  pop(vm); // Pop the array so we can remove the values
  while (length>0) { pop(vm); length--; } // Remove the values
  push(vm, array); // Finally push the array
}


// Pop index, pop Array, push the indexed Value
static bool arrayGetIndex(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0))) {
    runtimeError(vm, "Index must be a number.");
    return false;
  }
  if (!IS_ARRAY(peek(vm, 1))) {
    runtimeError(vm, "Can only subscript arrays.");
    return false;
  }

  int index = (int) AS_NUMBER(pop(vm));
  ObjArray* array = AS_ARRAY(pop(vm));

  if (index < 0 || index >= array->length ) {
    runtimeError(vm, "Index out of range.");
    return false;
  }
  push(vm, array->values[index]);
  return true;
}


// Pop Value, pop index, verify Array on stack, replace indexed value
static bool arraySetIndex(VM* vm) {
  Value value = pop(vm);
  if (!IS_NUMBER(peek(vm, 0))) {
    runtimeError(vm, "Index must be a number.");
    return false;
  }
  if (!IS_ARRAY(peek(vm, 1))) {
    runtimeError(vm, "Can only subscript arrays.");
    return false;
  }

  int index = (int) AS_NUMBER(pop(vm));
  ObjArray* array = AS_ARRAY(peek(vm, 0)); // Destination array is now on top of the stack

  if (index < 0 || index >= array->length ) {
    runtimeError(vm, "Index out of range.");
    return false;
  }

  array->values[index] = value;
  return true;
}


static bool arrayGetSlice(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) && !IS_NULL(peek(vm, 0))) {
    runtimeError(vm, "Invalid length type.");
    return false;
  }
  if (!IS_NUMBER(peek(vm, 1)) && !IS_NULL(peek(vm, 1))) {
    runtimeError(vm, "Invalid offset type.");
    return false;
  }
  if (!IS_ARRAY(peek(vm, 2))) {
    runtimeError(vm, "Can only slice arrays.");
    return false;
  }

  // Allocating a new array may trigger GC so just peek for now
  ObjArray* array = AS_ARRAY(peek(vm, 2));

  int offset = (IS_NULL(peek(vm, 1)) ? 0 : (int) AS_NUMBER(peek(vm, 1)));
  offset = check_offset(offset, array->length);
  if (offset == -1) { runtimeError(vm, "Offset out of range."); return false; }

  int length = IS_NULL(peek(vm, 0)) ? array->length - offset : (int) AS_NUMBER(peek(vm, 0));
  length = check_length(length, offset, array->length);
  if (length == -1) { runtimeError(vm, "Length out of range."); return false; }

  // Offset and length values are now within range and we are ready to allocate and copy
  ObjArray* result = newArray(vm);
  push(vm, OBJ_VAL(result)); // Store new array temporarily
  loadArray(vm, result, array->values + offset, length);
  pop(vm); // Pop new array temporarily so we can clean up

  // Pop length, offset and old array
  pop(vm);
  pop(vm);
  pop(vm);

  push(vm, OBJ_VAL(result));
  return true;
}


// Pop Array b, pop length, pop offset, splice Array a with b
static bool arraySetSlice(VM* vm) {
  if (!IS_ARRAY(peek(vm, 0))) {
    runtimeError(vm, "Splice source must be an array.");
    return false;
  }
  if (!IS_NUMBER(peek(vm, 1)) && !IS_NULL(peek(vm, 1))) {
    runtimeError(vm, "Invalid length type.");
    return false;
  }
  if (!IS_NUMBER(peek(vm, 2)) && !IS_NULL(peek(vm, 2))) {
    runtimeError(vm, "Invalid offset type.");
    return false;
  }
  if (!IS_ARRAY(peek(vm, 3))) {
    runtimeError(vm, "Splice destination must be an array.");
    return false;
  }

  // ALLOCATE may trigger GC so peek() instead of pop()
  ObjArray* b = AS_ARRAY(peek(vm, 0));
  ObjArray* a = AS_ARRAY(peek(vm, 3));

  int offset = (IS_NULL(peek(vm, 2)) ? 0 : (int) AS_NUMBER(peek(vm, 2)));
  offset = check_offset(offset, a->length);
  if (offset == -1) { runtimeError(vm, "Offset out of range."); return false; }

  int length = IS_NULL(peek(vm, 1)) ? a->length - offset : (int) AS_NUMBER(peek(vm, 1));
  length = check_length(length, offset, a->length);
  if (length == -1) { runtimeError(vm, "Length out of range."); return false; }

  // Offset and length values are now within range and we are ready to allocate and copy
  int newlen = a->length - length + b->length;
  Value* values = ALLOCATE(vm, Value, newlen);
  memcpy(values, a->values, offset * sizeof(Value)); // Copy prefix part of array "a" into new array
  memcpy(values + offset, b->values, b->length * sizeof(Value)); // Splice array "b" into new array
  memcpy(values + offset + b->length, a->values + offset + length, (a->length - (offset + length)) * sizeof(Value)); // Copy suffix part of array "a" into new array

  // Replace contents of the destination array
  FREE(vm, Value, a->values);
  a->length = newlen;
  a->values = values;

  // Pop array b, length and offset
  pop(vm);
  pop(vm);
  pop(vm);

  return true;
}

// Array + Array = Array
static void concatenateArrays(VM* vm) {
  //printf("vm:concatenateArrays() begin\n");
  // ALLOCATE may trigger GC so peek() instead of pop()
  ObjArray* b = AS_ARRAY(peek(vm, 0));
  ObjArray* a = AS_ARRAY(peek(vm, 1));

  int length = a->length + b->length;
  ObjArray* result = newArray(vm);
  push(vm, OBJ_VAL(result)); // Temporarily store new array to keep it safe
  result->values = ALLOCATE(vm, Value, length);
  result->length = length;
  memcpy(result->values, a->values, a->length * sizeof(Value)); // Copy array "a" into new array
  memcpy(result->values + a->length, b->values, b->length * sizeof(Value)); // Copy array "b" into new array
  pop(vm); // Pop new array so we can remove "a" and "b"

  // Now "a" and "b" can be popped safely
  pop(vm);
  pop(vm);
  push(vm, OBJ_VAL(result));
  //printf("vm:concatenateArrays() end\n");
}



static bool numberProperty(VM* vm, Value receiver, ObjString* name) {
  Value result;
  if (strncmp(name->chars, "base", name->length)==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, number_base));
    pop(vm);
    push(vm, result);
    return true;
  }
  runtimeError(vm, "Undefined property '%s'.", name->chars);
  return false;
}





VM* initVM() {
  VM* vm = malloc(sizeof(VM));

  resetStack(vm);
  vm->objects = NULL;
  vm->bytesAllocated = 0;
  vm->nextGC = 1024 * 1024;

  vm->sleep = 0;
  vm->yield = false;
  set_error_callback(vm, NULL);

  // GC graystack
  vm->grayCount = 0;
  vm->grayCapacity = 0;
  vm->grayStack = NULL;

  initTable(&vm->globals);
  initTable(&vm->strings);

  vm->parser = NULL;
  vm->compiler = NULL;

  defineNative(vm, "clock", clockNative);
  defineNative(vm, "sleep", sleepNative);
  defineNative(vm, "c_test", c_test);

  vm->errbuf = NULL;
  return vm;
}



InterpretResult run(VM* vm) {
  CallFrame* frame = &vm->frames[vm->frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))

//#define READ_CONSTANT()
//    (frame->closure->function->chunk.constants.values[READ_BYTE()])

#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
        runtimeError(vm, "Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      \
      double b = AS_NUMBER(pop(vm)); \
      double a = AS_NUMBER(pop(vm)); \
      push(vm, valueType(a op b)); \
    } while (false)


  // Timeslice start
  double start = now();
  double end;

  vm->yield = false; // Reset flag


  // Main loop
  // Replace with iterator or timed loop for cooperative execution?
  for (;;) {


    // Timeslice end?
    end = now();
    int delta_ms = (int) (1000.0 * (end - start));
    //printf("t=%d\n", delta_ms);
    if (delta_ms >= 10 || end < vm->sleep || vm->yield == true) return INTERPRET_RUNNING;



#ifdef DEBUG_TRACE_EXECUTION
    printf("        |    |    | ");
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
      if (IS_STRING(*slot)) {
        printf("[\"");
        printValue(*slot);
        printf("\"]");
      } else {
        printf("[");
        printValue(*slot);
        printf("]");
      }
    }
    printf("\n");
    disassembleInstruction(&frame->closure->function->chunk,
        (int)(frame->ip - frame->closure->function->chunk.code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(vm, constant);
        break;
      }
      case OP_NULL:     push(vm, NULL_VAL); break;
      case OP_TRUE:     push(vm, BOOL_VAL(true)); break;
      case OP_FALSE:    push(vm, BOOL_VAL(false)); break;
      case OP_POP:      pop(vm); break;
      case OP_POPN: {
        uint8_t count = READ_BYTE(); // Number of values to pop
        while (count>0) { pop(vm); count--; }
        break;
      }
      case OP_GET_LOCAL: {
//        uint8_t slot = READ_BYTE(); // Stack index from bottom
        uint16_t slot = READ_SHORT(); // Stack index from bottom
        push(vm, frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
//        uint8_t slot = READ_BYTE(); // Stack index from bottom
        uint16_t slot = READ_SHORT(); // Stack index from bottom
        frame->slots[slot] = peek(vm, 0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING(); // constant name from the bytecode
        Value value;
        if (!tableGet(vm, &vm->globals, name, &value)) {
          runtimeError(vm, "Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(vm, value);
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        if (tableSet(vm, &vm->globals, name, peek(vm, 0))) {
          tableDelete(vm, &vm->globals, name);
          runtimeError(vm, "Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING(); // constant no. from the bytecode
        tableSet(vm, &vm->globals, name, peek(vm, 0));
        pop(vm); // Don't pop value until safely stored
        break;
      }
      case OP_MAKE_ARRAY: { // EXPERIMENTAL
        uint8_t length = READ_BYTE();
        makeArray(vm, length);
        break;
      }
      case OP_GET_INDEX: { // EXPERIMENTAL
        if (arrayGetIndex(vm) == false) return INTERPRET_RUNTIME_ERROR;
        break;
      }
      case OP_SET_INDEX: { // EXPERIMENTAL
        if (arraySetIndex(vm) == false) return INTERPRET_RUNTIME_ERROR;
        break;
      }
      case OP_GET_SLICE: { // EXPERIMENTAL
        if (arrayGetSlice(vm) == false) return INTERPRET_RUNTIME_ERROR;
        break;
      }
      case OP_SET_SLICE: { // EXPERIMENTAL
        if (arraySetSlice(vm) == false) return INTERPRET_RUNTIME_ERROR;
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(vm, *frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(vm, 0);
        break;
      }
      case OP_GET_PROPERTY: {
        ObjString* name = READ_STRING();
        Value value = peek(vm, 0);
        Value result;
        if (strncmp(name->chars, "type", name->length)==0) {
          result = getValueType(vm, value);
          pop(vm);
          push(vm, result);
          return true;
        }
        if (IS_NUMBER(peek(vm, 0))) {
          if (numberProperty(vm, value, name) == false) { return INTERPRET_RUNTIME_ERROR; }
          break;
        }
        if (IS_STRING(peek(vm, 0))) {
          if (stringProperty(vm, value, name) == false) { return INTERPRET_RUNTIME_ERROR; }
          break;
        }
        if (IS_ARRAY(peek(vm, 0)))  {
          if (arrayProperty(vm, value, name)  == false) { return INTERPRET_RUNTIME_ERROR; }
          break;
        }
        if (!IS_INSTANCE(peek(vm, 0))) {
          runtimeError(vm, "Type does not have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjInstance* instance = AS_INSTANCE(peek(vm, 0));
        if (tableGet(vm, &instance->fields, name, &value)) {
          pop(vm); // Instance.
          push(vm, value);
          break;
        }
        runtimeError(vm, "Undefined property '%s'.", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(vm, 1))) {
          runtimeError(vm, "Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjInstance* instance = AS_INSTANCE(peek(vm, 1));
        tableSet(vm, &instance->fields, READ_STRING(), peek(vm, 0));
        Value value = pop(vm);
        pop(vm); // Instance
        push(vm, value);
        break;
      }
      case OP_EQUAL: {
        Value b = pop(vm);
        Value a = pop(vm);
        push(vm, BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_NEQUAL: {
        Value b = pop(vm);
        Value a = pop(vm);
        push(vm, BOOL_VAL(!valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
      case OP_GEQUAL:   BINARY_OP(BOOL_VAL, >=); break;
      case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
      case OP_LEQUAL:   BINARY_OP(BOOL_VAL, <=); break;
      case OP_DUP: {
        push(vm, peek(vm, 0));
        break;
      }
      case OP_INC: {
        if (IS_NUMBER(peek(vm, 0))) {
          double a = AS_NUMBER(pop(vm));
          push(vm, NUMBER_VAL(a + 1));
        } else {
          runtimeError(vm, "Can only increment numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_DEC: {
        if (IS_NUMBER(peek(vm, 0))) {
          double a = AS_NUMBER(pop(vm));
          push(vm, NUMBER_VAL(a - 1));
        } else {
          runtimeError(vm, "Can only decrement numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_ADD: {
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
          concatenateStrings(vm);
        } else if (IS_ARRAY(peek(vm, 0)) && IS_ARRAY(peek(vm, 1))) {
          concatenateArrays(vm);
        } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
          double b = AS_NUMBER(pop(vm));
          double a = AS_NUMBER(pop(vm));
          push(vm, NUMBER_VAL(a + b));
        } else {
          runtimeError(vm, "Operand types can not be added.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:
        BINARY_OP(NUMBER_VAL, /);
        if (isinf(AS_NUMBER(peek(vm, 0)))) {
          runtimeError(vm, "Division by zero.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      case OP_NOT:
        push(vm, BOOL_VAL(isFalsey(pop(vm))));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(vm, 0))) {
          runtimeError(vm, "Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
        break;
      case OP_BIN_NOT: {
        if (!IS_NUMBER(peek(vm, 0))) {
          runtimeError(vm, "Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
        push(vm, NUMBER_VAL((double) (~a)));
        break;
      }
      case OP_BIN_SHIFTL: {
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
          runtimeError(vm, "Both operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
        uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
        push(vm, NUMBER_VAL((double) (a << b)));
        break;
      }
      case OP_BIN_SHIFTR: {
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
          runtimeError(vm, "Both operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
        uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
        push(vm, NUMBER_VAL((double) (a >> b)));
        break;
      }
      case OP_BIN_AND: {
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
          runtimeError(vm, "Both operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
        uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
        push(vm, NUMBER_VAL((double) (a & b)));
        break;
      }
      case OP_BIN_OR: {
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
          runtimeError(vm, "Both operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
        uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
        push(vm, NUMBER_VAL((double) (a | b)));
        break;
      }
      case OP_BIN_XOR: {
        if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
          runtimeError(vm, "Both operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
        uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
        push(vm, NUMBER_VAL((double) (a ^ b)));
        break;
      }
      case OP_PRINT: {
        printValue(pop(vm));
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_PJMP_IF_FALSE: { // POP, then if false JUMP
        uint16_t offset = READ_SHORT();
        if (isFalsey(pop(vm))) frame->ip += offset;
        break;
      }
      case OP_QJMP_IF_FALSE: { // PEEK, then if false JUMP
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(vm, 0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        if (!callValue(vm, peek(vm, argCount), argCount)) {
          printf("vm:callValue() returned false\n");
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm->frames[vm->frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure* closure = newClosure(vm, function);
        push(vm, OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalueCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvalues[i] = captureUpvalue(vm, frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE: {
        closeUpvalues(vm, vm->stackTop - 1);
        pop(vm);
        break;
      }
      case OP_RETURN: {
        Value result = pop(vm);

        closeUpvalues(vm, frame->slots);

        vm->frameCount--;
        if (vm->frameCount == 0) {
          pop(vm);
          return INTERPRET_OK;
        }

        vm->stackTop = frame->slots;
        push(vm, result);

        frame = &vm->frames[vm->frameCount - 1];
        break;
      }
      case OP_CLASS:
        push(vm, OBJ_VAL(newClass(vm, READ_STRING())));
        break;
      default: {
        runtimeError(vm, "Internal error: unhandled OP_CODE %d.", instruction);
      }
    }
#ifdef DEBUG
    if (IS_NUMBER(peek(vm, 0)) && isinf(AS_NUMBER(peek(vm, 0)))) {
      runtimeError(vm, "Invalid number value.");
      return INTERPRET_RUNTIME_ERROR;
    }
#endif // DEBUG
  }


#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult includeFile(VM* vm, const char* fname, char** err) {
  // If already included, return INTERPRET_COMPILED to avoid circular references and redefines
  // ...

  // We need to search a set of "include" directories
  // ...

  // At this early testing stage we require FQFN

  char* source = readFile(fname);
  InterpretResult res = interpret(vm, source, err);
  printf("main:includeFile() free(%p) // source\n", (void*) source);
  free(source);
  return res;
}

InterpretResult processDirective(VM* vm, const char* line, int linelen, char** err) {
  InterpretResult res;
  printf("processDirective()\n");
  char* eos = ">"; // Enf-of-string characters
  // The following directives are supported:
  //  #include <filename>
  // All other directives are silently ignored
  if (linelen >= 10 && strncmp(line, "#include <", 10)==0) {
    printf("processDirective() #include\n");
    int fname_len = strcspn(line+10,eos);
    if (fname_len > 0) {
      char* fname = malloc(sizeof(char) * fname_len + 1);
      strncpy(fname, line+10, fname_len);
      fname[fname_len] = '\0';
      printf("processDirective() Including file \"%s\"\n", fname);
      res = includeFile(vm, fname, err);
      printf("main:processDirective() free(%p) // fname\n", (void*) fname);
      free(fname);
      if (res != INTERPRET_COMPILED) { return res; }
    } else {
      printf("#include directive must be followed by a filename\n");
      return INTERPRET_COMPILE_ERROR;
    }
  }
  return INTERPRET_COMPILED;
}

InterpretResult preprocess(VM* vm, const char* source, char** err) {
  // Quickly scan for lines that begin with # and process them as directives
  int srclen = strlen(source);
  int ln = 0;
  int first = 0;
  InterpretResult res;
  for (int i=0; i<srclen; i++) {
    if (source[i] == '\n') {
      if (source[first] == '#') {
        res = processDirective(vm, source+first, i-first, err);
        if (res != INTERPRET_COMPILED) { return res; }
      }
      ln++;
      first = i+1;
    }
  }
  return INTERPRET_COMPILED;
}

InterpretResult interpret(VM* vm, const char* source, char** err) {
  InterpretResult res;
  if (vm->errbuf != NULL) {
    free(vm->errbuf);
    vm->errbuf = NULL;
  }
  res = preprocess(vm, source, &vm->errbuf); // May call interpret() recursively if #include directives are encountered
  *err = vm->errbuf; // Give caller a pointer to the buffer
  if (res != INTERPRET_COMPILED) { return res; }

  int fileno = -1; // Temp
  ObjFunction* function = compile(vm, fileno, source, err);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(vm, OBJ_VAL(function));
  ObjClosure* closure = newClosure(vm, function);
  pop(vm);
  push(vm, OBJ_VAL(closure));
  callValue(vm, OBJ_VAL(closure), 0);

  return INTERPRET_COMPILED;
  //return run();
}



