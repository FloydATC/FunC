#include <stdio.h>
#include <string.h>
#include <math.h>

#include "objnumber.h"

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





bool numberProperty(void* vm, Value receiver, ObjString* name) {
  Value result;
  if (strncmp(name->chars, "floor", name->length)==0) {
    // Return floor() of a decimal number
    result = NUMBER_VAL(floor(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "ceil", name->length)==0) {
    // Return ceil() of a decimal number
    result = NUMBER_VAL(ceil(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "sqrt", name->length)==0) {
    // Return sqrt() of a decimal number
    result = NUMBER_VAL(sqrt(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "abs", name->length)==0) {
    // Return abs() of a decimal number
    result = NUMBER_VAL(fabs(AS_NUMBER(receiver)));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "base", name->length)==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, number_base));
    pop(vm);
    push(vm, result);
    return true;
  }
  runtimeError(vm, "Undefined property '%s'.", name->chars);
  return false;
}
