#include <stdio.h>
#include <string.h>
#include <math.h>

#include "objnumber.h"

#include "memory.h"
#include "number.h"
#include "vm.h"


// Native C method: NUMBER.base()
// NOTE: This temporary implementation does not support decimals
// rewrite to use double_to_str() in number.h when it has been implemented
static bool number_base(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1) {
    runtimeError(vm, "Method needs one argument.");
    return false;
  }
  if (!IS_NUMBER(args[0])) {
    runtimeError(vm, "Argument must be a number.");
    return false;
  }
  int radix = AS_NUMBER(args[0]);
  double number = AS_NUMBER(receiver);
  char* string = NULL;
  int length = double_to_str(number, &string, radix);

  *result = OBJ_VAL(copyString(vm, string, length));
  return true;
}


// Native C method: NUMBER.f()
// Allows the user manual control over %f or %g number formatting
static bool number_format(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1 || !IS_STRING(args[0])) {
    runtimeError(vm, "Method needs 1 string argument.");
    return false;
  }
  char* format = AS_CSTRING(args[0]);
  int length = snprintf(NULL, 0, format, AS_NUMBER(receiver));
  char* string = ALLOCATE(vm, char, length + 1);
  length = snprintf(string, length, format, AS_NUMBER(receiver));

  *result = OBJ_VAL(copyString(vm, string, length));
  return true;
}




bool numberProperty(void* vm, Value receiver, ObjString* name) {
  Value result;
  if (strcmp(name->chars, "floor")==0) {
    // Return floor() of a decimal number
    result = NUMBER_VAL(floor(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "ceil")==0) {
    // Return ceil() of a decimal number
    result = NUMBER_VAL(ceil(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "sqrt")==0) {
    // Return sqrt() of a decimal number
    result = NUMBER_VAL(sqrt(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "abs")==0) {
    // Return abs() of a decimal number
    result = NUMBER_VAL(fabs(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "base")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, number_base));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strcmp(name->chars, "f")==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, number_format));
    pop(vm);
    push(vm, result);
    return true;
  }
  runtimeError(vm, "Undefined property '%s'.", name->chars);
  return false;
}
