#include <stdio.h>
#include <string.h>
#include <math.h>

#include "objnumber.h"

#include "memory.h"
#include "number.h"
#include "utf8.h"
#include "vm.h"

#define CHECK_ARG_IS_STRING(index) \
  if (argCount >= index+1 && !IS_STRING(args[index])) { \
    runtimeError(vm, "Argument %d must be a string, got %s.", index+1, getValueTypeString(args[index])); \
    return false; \
  }

#define CHECK_ARG_IS_NUMBER(index) \
  if (argCount >= index+1 && !IS_NUMBER(args[index])) { \
    runtimeError(vm, "Argument %d must be a number, got %s.", index+1, getValueTypeString(args[index])); \
    return false; \
  }

#define CHECK_ARGS_ZERO_OR_ONE() \
  if (argCount > 1) { \
    runtimeError(vm, "Method takes 1 optional argument, got %d.", argCount); \
    return false; \
  }

#define CHECK_ARGS_ONE_OR_TWO() \
  if (argCount < 1 || argCount > 2) { \
    runtimeError(vm, "Method takes 1 or 2 arguments, got %d.", argCount); \
    return false; \
  }

#define CHECK_ARGS_ONE() \
  if (argCount != 1) { \
    runtimeError(vm, "Method takes 1 argument, got %d.", argCount); \
    return false; \
  }

// ==== attributes ====

// Native C attribute: NUMBER.char
static bool number_char(void* vm, Value receiver, Value* result) {

  // Return character of a utf8 codepoint
  int bufsiz = 5;
  char buf[bufsiz];
  uint32_t codepoint = (uint32_t) AS_NUMBER(receiver);
  int length = u8_toutf8(buf, bufsiz, &codepoint, 1);

  *result = OBJ_VAL(copyString(vm, buf, length));
  return true;

}


// ==== methods ====

// Native C method: NUMBER.base()
static bool number_base(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();
  CHECK_ARG_IS_NUMBER(0);

  double number = AS_NUMBER(receiver);
  int radix = AS_NUMBER(args[0]);

  char* string = NULL;
  int length = double_to_str(number, &string, radix);
  //printf("objnumber:number_base() string=%s, length=%d\n", string, length);

  *result = OBJ_VAL(takeString(vm, string, length));
  return true;
}


// Native C method: NUMBER.atan2()
static bool number_atan2(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();
  CHECK_ARG_IS_NUMBER(0);

  double y = AS_NUMBER(receiver);
  double x = AS_NUMBER(args[0]);

  *result = NUMBER_VAL(atan2(y, x)); // Note the reversed order
  return true;
}

// Native C method: NUMBER.pow()
static bool number_pow(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();
  CHECK_ARG_IS_NUMBER(0);

  double x = AS_NUMBER(receiver);
  double y = AS_NUMBER(args[0]);

  *result = NUMBER_VAL(pow(x, y));
  return true;
}

// Native C method: NUMBER.fmod()
static bool number_fmod(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();
  CHECK_ARG_IS_NUMBER(0);

  double x = AS_NUMBER(receiver);
  double y = AS_NUMBER(args[0]);

  *result = NUMBER_VAL(fmod(x, y));
  return true;
}

// Native C method: NUMBER.hypot()
static bool number_hypot(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();
  CHECK_ARG_IS_NUMBER(0);

  double x = AS_NUMBER(receiver);
  double y = AS_NUMBER(args[0]);

  *result = NUMBER_VAL(hypot(x, y));
  return true;
}


#define PROPERTY(fn_name,fn_call) \
  if (strcmp(name->chars, fn_name)==0) { \
    *property = NUMBER_VAL(fn_call(AS_NUMBER(receiver))); \
    return true; \
  } \

#define METHOD(fn_name, fn_call) \
  if (strcmp(name->chars, fn_name)==0) { \
    *property = OBJ_VAL(newNativeMethod(vm, receiver, name, fn_call)); \
    return true; \
  }


bool getNumberProperty(void* vm, Value receiver, ObjString* name, Value* property) {

  if (strcmp(name->chars, "char")==0) return number_char(vm, receiver, property);

  // These properties all call a native C function, take no arguments and return a number
  PROPERTY("sin",   sin);
  PROPERTY("cos",   cos);
  PROPERTY("tan",   tan);
  PROPERTY("asin",  asin);
  PROPERTY("acos",  acos);
  PROPERTY("atan",  atan);
  PROPERTY("sinh",  sinh);
  PROPERTY("cosh",  cosh);
  PROPERTY("tanh",  tanh);
  PROPERTY("asinh", asinh);
  PROPERTY("acosh", acosh);
  PROPERTY("atanh", atanh);
  PROPERTY("exp",   exp);
  PROPERTY("log",   log);
  PROPERTY("log10", log10);
  PROPERTY("floor", floor);
  PROPERTY("ceil",  ceil);
  PROPERTY("sqrt",  sqrt);
  PROPERTY("cbrt",  cbrt);
  PROPERTY("abs",   abs);

  // These methods all call a C function defined above, take 1 argument and return a number
  METHOD("base",  number_base);
  METHOD("atan2", number_atan2);
  METHOD("pow",   number_pow);
  METHOD("fmod",  number_fmod);
  METHOD("hypot", number_hypot);

  runtimeError(vm, "Number has no '%s'.", name->chars);
  return false;
}


// Get a number property, pop receiver and push the property
bool pushNumberProperty(void* vm, Value receiver, ObjString* name) {
  Value method;
  if (!getNumberProperty(vm, receiver, name, &method)) return false;
  pop(vm); // receiver
  push(vm, method);
  return true;
}



#undef PROPERTY
#undef METHOD

#undef CHECK_ARG_IS_STRING
#undef CHECK_ARGS_ZERO_OR_ONE
#undef CHECK_ARGS_ONE_OR_TWO
#undef CHECK_ARGS_ONE

