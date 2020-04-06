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
#include "objnumber.h"
#include "memory.h"

#ifdef _MSC_VER
//#include <sysinfoapi.h>
#include <windows.h>
#endif

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
    int fileno = function->chunk.files[ip];
    char* filename = AS_CSTRING(getFilenameByNo(vm, fileno));
    int lineno = function->chunk.lines[ip];
    int charno = function->chunk.chars[ip];
    if (function->name == NULL) {
      msgbuf = bprintf(msgbuf, "at %s[%d:%d]\n", filename, lineno, charno);
    } else {
      char* fn = function->name->chars;
      msgbuf = bprintf(msgbuf, "%s at %s[%d:%d]\n", fn, filename, lineno, charno);
    }

  }

  // Print buffer on stderr
  fprintf(stderr, "%s", msgbuf);

  // Callback with buffer
  if (vm->error_callback != NULL) { vm->error_callback(msgbuf); }

  // Deallocate error message buffer
  printf("vm:runtimeError() free(%p) // message buffer\n", (void*) msgbuf);
  free(msgbuf);

  resetStack(vm);
}



// Helper functions for the API

Value nullValue() {
  return NULL_VAL;
}

Value to_numberValue(double n) {
  return NUMBER_VAL(n);
}


Value to_nativeValue(VM* vm, const char* name, NativeFn function) {
  ObjString* name_obj = copyString(vm, name, (int)strlen(name));
  push(vm, OBJ_VAL(name_obj));
  Value native = OBJ_VAL(newNative(vm, name_obj, function));
  push(vm, native);
  // We must store this somewhere safe before returning
  size_t taglen = 1 + strlen(name); // +1 for prefix "*"
  char* tagname = ALLOCATE(vm, char, taglen + 1); // +1 for terminator
  tagname[0] = '*';
  strncpy(tagname+1, name, strlen(name));
  tagname[taglen] = '\0';
  defineGlobal(vm, tagname, native); // "*name" = native
  pop(vm); // native
  pop(vm); // name_obj
  FREE(vm, char, tagname);
  return native;
}

// API function: return a "native" object instance populated with fields
Value to_instanceValue(VM* vm, const char** fields, Value* values, int length) {
  // Make sure the values are safe from GC
  for (int i=0; i<length; i++) push(vm, values[i]);

  //printf("vm:to_instanceValue() constructing instance with %d member values\n", length);
  // First we create an empty "native" class using a name that users can't refer to
  ObjString* klassname = copyString(vm, "*", 1);
  push(vm, OBJ_VAL(klassname));
  ObjClass* klass = newClass(vm, klassname);
  push(vm, OBJ_VAL(klass)); // Store temporarily
  // Instantiate the class
  ObjInstance* instance = newInstance(vm, klass);
  push(vm, OBJ_VAL(instance)); // Store temporarily
  // Now populate the instance with the fields/values specified
  for (int i=0; i<length; i++) {
    ObjString* fieldname = copyString(vm, fields[i], (int) strlen(fields[i]));
    push(vm, OBJ_VAL(fieldname));
    tableSet(vm, &instance->fields, fieldname, values[i]);
    //printf("vm:to_instanceValue() member %d name=%s value=%s\n", i, fields[i], getValueTypeString(values[i]));
    pop(vm); // fieldname is now referenced by the instance, which is on the stack
  }
  pop(vm); // instance
  pop(vm); // klass
  pop(vm); // klassname

  // The values are now safe from GC
  for (int i=0; i<length; i++) pop(vm);

  return OBJ_VAL(instance);
}

Value to_stringValueArray(VM* vm, const char** cstr, int array_length) {
  // We will use the VM's stack to temporarily hold each string value,
  // both to prevent them from being garbage collected, and to create the array
  for (int i=0; i<array_length; i++) {
    ObjString* obj = copyString(vm, cstr[i], (int) strlen(cstr[i]));
    push(vm, OBJ_VAL(obj));
  }
  makeArray(vm, array_length);
  return pop(vm);
}

Value to_numberValueArray(VM* vm, double* number, int array_length) {
  // We will use the VM's stack to temporarily hold each number value,
  // so we can use the VM's internal function to create the array
  for (int i=0; i<array_length; i++) {
    push(vm, NUMBER_VAL(number[i]));
  }
  makeArray(vm, array_length);
  return pop(vm);
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
#define NUMBER_BUFSIZ 1024
    static char x[NUMBER_BUFSIZ];
    snprintf(x, NUMBER_BUFSIZ, "%g", AS_NUMBER(v));
#undef NUMBER_BUFSIZ
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


#ifdef _MSC_VER
#define CLOCK_REALTIME -1
//struct timespec { long tv_sec; long tv_nsec; };    //header part
int clock_gettime(int ignore, struct timespec* spec)      //C-file part
{
    __int64 wintime; GetSystemTimeAsFileTime((LPFILETIME)&wintime);
    wintime -= 116444736000000000i64;  //1jan1601 to 1jan1970
    spec->tv_sec = wintime / 10000000i64;           //seconds
    spec->tv_nsec = wintime % 10000000i64 * 100;      //nano-seconds
    return 0;
}
#endif

double now() {
  long            ms; // Milliseconds
  time_t          s;  // Seconds
  struct timespec spec;
  double ts;
  clock_gettime(CLOCK_REALTIME, &spec);

  s  = spec.tv_sec;
  ms = (long)round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
  if (ms > 999) {
    s++;
    ms = 0;
  }
  ts = s + (ms/1000.0);
  //printf("now() = %f\n", ts);
  return ts;
}



// Native C function: clock()
static bool clockNative(void* vm, int argCount, Value* args, Value* result) {
  (unused)vm;
  (unused)argCount;
  (unused)args;
  *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC); // time.h
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




void set_error_callback(VM* vm, ErrorCb ptr) {
  vm->error_callback = ptr;
}


// API function: Add a named value to the global namespace
void defineGlobal(VM* vm, const char* name, Value value) {
  push(vm, value); // Store temporarily
  ObjString* name_obj = copyString(vm, name, (int)strlen(name));
  push(vm, OBJ_VAL(name_obj)); // Store temporarily
  tableSet(vm, &vm->globals, name_obj, value);
  pop(vm); // name_obj
  pop(vm); // value
  return;
}

// API function: Create a named native function and add it to the global namespace
// DEPRECATED: use to_nativeValue() + defineGlobal() instead
void defineNative(VM* vm, const char* name, NativeFn function) {
  ObjString* name_obj = copyString(vm, name, (int)strlen(name));
  push(vm, OBJ_VAL(name_obj));
  Value native = OBJ_VAL(newNative(vm, name_obj, function));
  push(vm, native);
  tableSet(vm, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
  pop(vm);
  pop(vm);
  return;
}

void freeVM(VM* vm) {
  freeValueArray(vm, &vm->filenames);
  //printf("vm.freeVM(%p) freeing globals\n", (void*)vm);
  freeTable(vm, &vm->globals);
  //printf("vm.freeVM(%p) freeing strings\n", (void*)vm);
  freeTable(vm, &vm->strings);
  //printf("vm.freeVM(%p) freeing objects\n", (void*)vm);
  vm->initString = NULL; // Gets freed by freeObjects
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



static bool callValue(VM* vm, Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm->stackTop[-argCount - 1] = bound->receiver; // Receiver will go in stack slot zero
        return call(vm, bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm->stackTop[-argCount - 1] = OBJ_VAL(newInstance(vm, klass));
        Value initializer;
        if (tableGet(vm, &klass->methods, vm->initString, &initializer)) {
          return call(vm, AS_CLOSURE(initializer), argCount);
        } else if (argCount != 0) {
          runtimeError(vm, "Expected 0 arguments, got %d.", argCount);
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE: {
        return call(vm, AS_CLOSURE(callee), argCount);
      }
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee)->function;
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
      case OBJ_NATIVE_METHOD: {
        ObjNativeMethod* method = AS_NATIVE_METHOD(callee);
        //printf("vm:callValue() will call %p with argCount=%d, args=%p\n", method, argCount,(vm->stackTop - argCount));
        Value result;
        //printf("vm:callValue() args ok, calling\n");
        bool success = method->function(vm, method->receiver, argCount, vm->stackTop - argCount, &result);
        //printf("vm:callValue() returned from native function\n");
        if (!success) {
          runtimeError(vm, "Call failed, check arguments.");
          return false;
        }
        vm->stackTop -= argCount + 1; // Discard arguments from stack
        //printf("vm:callValue() pushing result to stack\n");
        push(vm, result);
        return true;
      }
      default:
        // Non-callable object type.
        break;
    }
  }

  runtimeError(vm, "Can only call functions and classes.");
  return false;
} // callValue


static bool invokeFromArray(VM* vm, Value receiver, ObjString* name, int argCount) {
  Value method;
  if (!getArrayProperty(vm, receiver, name, &method)) {
    runtimeError(vm, "(Array) has no '%s'.", name->chars);
    return false;
  }
  return callValue(vm, method, argCount);
}


static bool invokeFromNumber(VM* vm, Value receiver, ObjString* name, int argCount) {
  Value method;
  if (!getNumberProperty(vm, receiver, name, &method)) {
    runtimeError(vm, "(Number) has no '%s'.", name->chars);
    return false;
  }
  return callValue(vm, method, argCount);
}


static bool invokeFromString(VM* vm, Value receiver, ObjString* name, int argCount) {
  Value method;
  if (!getStringProperty(vm, receiver, name, &method)) {
    runtimeError(vm, "(String) has no '%s'.", name->chars);
    return false;
  }
  return callValue(vm, method, argCount);
}


static bool invokeFromClass(VM* vm, ObjClass* klass, ObjString* name, int argCount) {
  Value method;
  if (!tableGet(vm, &klass->methods, name, &method)) {
    runtimeError(vm, "%s has no '%s'.", &klass->name->chars, name->chars);
    return false;
  }

  return call(vm, AS_CLOSURE(method), argCount);
}


static bool invoke(VM* vm, ObjString* name, int argCount) {
  Value receiver = peek(vm, argCount); // Stack slot zero
  //printf("vm:invoke() name=%s receiver=", name->chars);
  //printValue(receiver);
  //printf(" argCount=%d\n", argCount);

  // Check built-in properties
  if (IS_ARRAY(receiver)) return invokeFromArray(vm, receiver, name, argCount);
  if (IS_NUMBER(receiver)) return invokeFromNumber(vm, receiver, name, argCount);
  if (IS_STRING(receiver)) return invokeFromString(vm, receiver, name, argCount);

  if (!IS_INSTANCE(receiver)) {
    //runtimeError(vm, "Only instances have methods. #invoke");
    runtimeError(vm, "Type %s has no properties.", getValueTypeString(receiver));
    return false;
  }

  ObjInstance* instance = AS_INSTANCE(receiver);

  // Fields have precedence over methods, and a field may contain a plain function
  Value value;
  if (tableGet(vm, &instance->fields, name, &value)) {
    vm->stackTop[-argCount - 1] = value;
    return callValue(vm, value, argCount);
  }

  // Invoke as method
  return invokeFromClass(vm, instance->klass, name, argCount);
}



static bool bindMethod(VM* vm, ObjClass* klass, ObjString* name) {
  Value method;
  if (!tableGet(vm, &klass->methods, name, &method)) {
    runtimeError(vm, "%s has no '%s'.", &klass->name->chars, name->chars);
    return false;
  }

  ObjBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(method));
  pop(vm);
  push(vm, OBJ_VAL(bound));
  return true;
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

static void defineMethod(VM* vm, ObjString* name) {
  Value method = peek(vm, 0);
  ObjClass* klass = AS_CLASS(peek(vm, 1));
  tableSet(vm, &klass->methods, name, method);
  pop(vm);
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
  //printf("vm:concatenateString()\n");
  // takeString() may trigger GC so peek() instead of pop()
  ObjString* b = AS_STRING(peek(vm, 0));
  ObjString* a = AS_STRING(peek(vm, 1));

  int length = a->length + b->length;
  //printf("vm:concatenateString() length = %d + %d = %d\n", a->length, b->length, length);
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
void makeArray(VM* vm, uint8_t length) {
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

  initValueArray(&vm->filenames); // Experimental include support

  //vm->parser = NULL;
  vm->compiler = NULL;
  vm->currentClass = NULL;

  vm->initString = NULL;
  vm->initString = copyString(vm, "init", 4);

  defineNative(vm, "clock", clockNative);
  defineNative(vm, "sleep", sleepNative);

  return vm;
}


bool op_divide(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
    runtimeError(vm, "Operands must be numbers.");
    return false;
  }
  double b = AS_NUMBER(pop(vm));
  double a = AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL(a / b));
  if (isinf(AS_NUMBER(peek(vm, 0)))) {
    runtimeError(vm, "Division by zero.");
    return false;
  }
  return true;
}

bool op_multiply(VM* vm) {
  // Multiplication may mean different things depending on value types
  //printf("vm:op_multiply()\n");
  if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
    //printf("vm:op_multiply() both operands are numbers\n");
    // NUMBER * NUMBER = simple multiplication
    double b = AS_NUMBER(pop(vm));
    double a = AS_NUMBER(pop(vm));
    push(vm, NUMBER_VAL(a * b));
    return true;
  }

  if (IS_NUMBER(peek(vm, 1)) && IS_STRING(peek(vm, 0))) {
    // NUMBER * STRING = repeat string
    // We prefer the operands in reverse so we swap them
    Value b = pop(vm);
    Value a = pop(vm);
    push(vm, b);
    push(vm, a);
    // The repetition will be handled below
  }

  if (IS_NUMBER(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
    // STRING * NUMBER = repeat string
    ObjString* a = AS_STRING(peek(vm, 1));
    int b = (int) AS_NUMBER(peek(vm, 0));
    int length = a->length * b;
    //printf("vm:op_multiply() repeat string '%s' %d times = %d bytes \n", a->chars, b, length);
    // Allocate a temp buffer
    char* tmp = ALLOCATE(vm, char, length + 1);
    for (int i=0; i<b; i++) {
      memcpy(tmp+(i*a->length), a->chars, a->length);
    }
    tmp[length] = '\0'; // Terminate string
    // Let the memory manager take ownership of the buffer;
    // it will either be interned as an ObjString or discarded
    // Either way, we get a valid ObjString pointer in return
    ObjString* result = takeString(vm, tmp, length);
    // We have finished allocating memory so it's safe to pop the values
    pop(vm);
    pop(vm);
    push(vm, OBJ_VAL(result));
    return true;
  } else {
    runtimeError(vm, "Operands can not be multiplied.");
    return false;
  }
  return true;
}

bool op_add(VM* vm) {
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
    return false;
  }
  return true;
}


bool op_modulo(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
    runtimeError(vm, "Operand must be a number.");
    return false;
  }
  int b = (int) AS_NUMBER(pop(vm));
  int a = (int) AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL(a % b));
  return true;
}


bool op_inc(VM* vm) {
  if (IS_NUMBER(peek(vm, 0))) {
    double a = AS_NUMBER(pop(vm));
    push(vm, NUMBER_VAL(a + 1));
  } else {
    runtimeError(vm, "Can only increment numbers.");
    return false;
  }
  return true;
}


bool op_dec(VM* vm) {
  if (IS_NUMBER(peek(vm, 0))) {
    double a = AS_NUMBER(pop(vm));
    push(vm, NUMBER_VAL(a - 1));
  } else {
    runtimeError(vm, "Can only decrement numbers.");
    return false;
  }
  return true;
}


bool op_negate(VM* vm) {
  if (IS_NUMBER(peek(vm, 0))) {
    push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
  } else if(IS_BOOL(peek(vm, 0))) {
    push(vm, BOOL_VAL(!AS_BOOL(pop(vm))));
  } else {
    runtimeError(vm, "Operand must be a number or boolean.");
    return false;
  }
  return true;
}


bool op_bin_not(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0))) {
    runtimeError(vm, "Operand must be a number.");
    return false;
  }
  uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL((double) (~a)));
  return true;
}


bool op_bin_shiftl(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
    runtimeError(vm, "Both operands must be numbers.");
    return false;
  }
  uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
  uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL((double) (a << b)));
  return true;
}


bool op_bin_shiftr(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
    runtimeError(vm, "Both operands must be numbers.");
    return false;
  }
  uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
  uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL((double) (a >> b)));
  return true;
}


bool op_bin_and(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
    runtimeError(vm, "Both operands must be numbers.");
    return false;
  }
  uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
  uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL((double) (a & b)));
  return true;
}

bool op_bin_or(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
    runtimeError(vm, "Both operands must be numbers.");
    return false;
  }
  uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
  uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL((double) (a | b)));
  return true;
}

bool op_bin_xor(VM* vm) {
  if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) {
    runtimeError(vm, "Both operands must be numbers.");
    return false;
  }
  uint32_t b = (uint32_t) AS_NUMBER(pop(vm));
  uint32_t a = (uint32_t) AS_NUMBER(pop(vm));
  push(vm, NUMBER_VAL((double) (a ^ b)));
  return true;
}


bool op_equal(VM* vm) {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(valuesEqual(a, b)));
  return true;
}

bool op_nequal(VM* vm) {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(!valuesEqual(a, b)));
  return true;
}

bool op_greater(VM* vm) {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(valuesGreater(a, b)));
  return true;
}

bool op_gequal(VM* vm) {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(valuesEqual(b, a) || valuesGreater(a, b)));
  return true;
}

bool op_less(VM* vm) {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(valuesGreater(b, a))); // Note: reverse
  return true;
}

bool op_lequal(VM* vm) {
  Value b = pop(vm);
  Value a = pop(vm);
  push(vm, BOOL_VAL(valuesEqual(b, a) || valuesGreater(b, a))); // Note: reverse
  return true;
}

bool op_not(VM* vm) {
  push(vm, BOOL_VAL(isFalsey(pop(vm))));
  return true;
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

#define CALL_OP(function) \
    do { \
      if (function(vm) == false) return INTERPRET_RUNTIME_ERROR; \
    } while (false)

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

        // Check built-in properties -- OP_INVOKE must do the same
        if (IS_ARRAY(peek(vm, 0))) return pushArrayProperty(vm, value, name);
        if (IS_NUMBER(peek(vm, 0))) return pushNumberProperty(vm, value, name);
        if (IS_STRING(peek(vm, 0))) return pushStringProperty(vm, value, name);

        if (!IS_INSTANCE(peek(vm, 0))) {
          runtimeError(vm, "Type %s has no properties.", getValueTypeString(value));
          return INTERPRET_RUNTIME_ERROR;
        }
        // If we get this far, the receiver should be an object
        // Fields take precedence so check those first
        ObjInstance* instance = AS_INSTANCE(peek(vm, 0));
        if (tableGet(vm, &instance->fields, name, &value)) {
          pop(vm); // Instance.
          push(vm, value);
          break;
        }
        // Next, check if name refers to a method
        if (!bindMethod(vm, instance->klass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
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
      case OP_GET_SUPER: {
        ObjString* name = READ_STRING();
        ObjClass* superclass = AS_CLASS(pop(vm));
        if (!bindMethod(vm, superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_EQUAL:      CALL_OP(op_equal); break;
      case OP_NEQUAL:     CALL_OP(op_nequal); break;
      case OP_GREATER:    CALL_OP(op_greater); break;
      case OP_GEQUAL:     CALL_OP(op_gequal); break;
      case OP_LESS:       CALL_OP(op_less); break;
      case OP_LEQUAL:     CALL_OP(op_lequal); break;
      case OP_DUP:        push(vm, peek(vm, 0)); break;
      case OP_INC:        CALL_OP(op_inc); break;
      case OP_DEC:        CALL_OP(op_dec); break;
      case OP_ADD:        CALL_OP(op_add); break;
      case OP_SUBTRACT:   BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY:   CALL_OP(op_multiply); break;
      case OP_DIVIDE:     CALL_OP(op_divide); break;
      case OP_MODULO:     CALL_OP(op_modulo); break;
      case OP_NOT:        CALL_OP(op_not); break;
      case OP_NEGATE:     CALL_OP(op_negate); break;
      case OP_BIN_NOT:    CALL_OP(op_bin_not); break;
      case OP_BIN_SHIFTL: CALL_OP(op_bin_shiftl); break;
      case OP_BIN_SHIFTR: CALL_OP(op_bin_shiftr); break;
      case OP_BIN_AND:    CALL_OP(op_bin_and); break;
      case OP_BIN_OR:     CALL_OP(op_bin_or); break;
      case OP_BIN_XOR:    CALL_OP(op_bin_xor); break;
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
      case OP_INVOKE: {
        // = OP_GET_PROPERTY + OP_CALL combined
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        if (!invoke(vm, method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm->frames[vm->frameCount - 1];
        break;
      }
      case OP_SUPER_INVOKE: {
        ObjString* method = READ_STRING();
        int argCount = READ_BYTE();
        ObjClass* superclass = AS_CLASS(pop(vm));
        if (!invokeFromClass(vm, superclass, method, argCount)) {
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
      case OP_EXIT: {
        vm->stackTop = vm->stack;
        vm->frameCount = 0;
        return INTERPRET_OK;
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
      case OP_INHERIT: {
        Value superclass = peek(vm, 1);
        if (!IS_CLASS(superclass)) {
          runtimeError(vm, "Superclass must be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjClass* subclass = AS_CLASS(peek(vm, 0));
        tableAddAll(vm, &AS_CLASS(superclass)->methods, &subclass->methods);
        pop(vm); // Subclass
        break;
      }
      case OP_METHOD:
        defineMethod(vm, READ_STRING());
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



static ObjFunction* interpret_inner(VM* vm, const char* source, const char* filename) {
  int fileno = -1;
  if (strlen(filename) > 0) fileno = addFilename(vm, filename);
  return compile(vm, fileno, source);
}

static void setup_initial_callframe(VM* vm, ObjFunction* function) {
  // Set up the very first CallFrame to begin executing the top level code of the script
  push(vm, OBJ_VAL(function));
  ObjClosure* closure = newClosure(vm, function);
  pop(vm);
  push(vm, OBJ_VAL(closure));
  callValue(vm, OBJ_VAL(closure), 0);
  return;
}

InterpretResult interpret(VM* vm, const char* source, const char* filename) {
  ObjFunction* function = interpret_inner(vm, source, filename);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;
  //printf("====== compilation complete ======\n");
  setup_initial_callframe(vm, function);
  return INTERPRET_COMPILED;
}



