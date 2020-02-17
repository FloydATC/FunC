#include <stdio.h>
#include <string.h>
#include <math.h>

#include "objnumber.h"

#include "memory.h"
#include "number.h"
#include "utf8.h"
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


/*

sin
cos
tan
asin
acos
atan
atan2
sinh
cosh
tanh
exp
log
log10
pow
fmod
hypot

*/


bool numberProperty(void* vm, Value receiver, ObjString* name) {
  Value result;
  if (strcmp(name->chars, "char")==0) {
    // Return character of a utf8 codepoint
    int bufsiz = 5;
    char buf[bufsiz];
    uint32_t codepoint = (uint32_t) AS_NUMBER(receiver);
    int length = u8_toutf8(buf, bufsiz, &codepoint, 1);
    pop(vm);
    push(vm, OBJ_VAL(copyString(vm, buf, length)));
    return true;
  }
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
  runtimeError(vm, "Number has no '%s'.", name->chars);
  return false;
}
