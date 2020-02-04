#include <string.h>

#include "objarray.h"
#include "memory.h"
#include "object.h"
#include "vm.h"



// Native C method: ARRAY.shift()
static bool array_shift(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  (unused)args; // Shift does not take any arguments
  if (argCount > 0) {
    runtimeError(vm, "Method does not take arguments.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);
  Value value = NULL_VAL;

  if (array->length > 0) {
    int length = array->length - 1;
    value = array->values[0]; // Get the first entry
    Value* values = ALLOCATE(vm, Value, length); // Create new array buffer
    memcpy(values, array->values + 1, length * sizeof(Value)); // Copy array values except first Value into new array buffer
    FREE(vm, Value, array->values); // Dispose of the old array buffer
    array->length = length;
    array->values = values; // Assign the new buffer
  }

  *result = value;
  return true;
}



// Native C method: ARRAY.unshift()
static bool array_unshift(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount == 0) {
    runtimeError(vm, "Method needs atleast one argument.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);

  int length = array->length + argCount;
    Value* values = ALLOCATE(vm, Value, length); // Create new array buffer
    for (int i=0; i<argCount; i++) {
      // Copy new value(s) into new array buffer in reverse order
      memcpy(values + i, args + argCount - i - 1, sizeof(Value));
    }
    memcpy(values + argCount, array->values, array->length * sizeof(Value)); // Copy array values into new array buffer
    FREE(vm, Value, array->values); // Dispose of the old array buffer
    array->length = length;
    array->values = values; // Assign the new buffer

  *result = receiver;
  return true;
}


// Native C method: ARRAY.pop()
static bool array_pop(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  (unused)args; // pop() does not take any arguments
  if (argCount > 0) {
    runtimeError(vm, "Method does not take arguments.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);
  Value value = NULL_VAL;

  if (array->length > 0) {
    int length = array->length - 1;
    value = array->values[array->length - 1]; // Get the last entry
    Value* values = ALLOCATE(vm, Value, length); // Create new array buffer
    memcpy(values, array->values, length * sizeof(Value)); // Copy array values except last Value into new array buffer
    FREE(vm, Value, array->values); // Dispose of the old array buffer
    array->length = length;
    array->values = values; // Assign the new buffer
  }

  *result = value;
  return true;
}


// Native C method: ARRAY.push()
static bool array_push(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount == 0) {
    runtimeError(vm, "Method needs atleast one argument.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);

  int length = array->length + argCount;
    Value* values = ALLOCATE(vm, Value, length); // Create new array buffer
    memcpy(values, array->values, array->length * sizeof(Value)); // Copy array values into new array buffer
    memcpy(values + array->length, args, argCount * sizeof(Value)); // Copy new value(s) into new array buffer
    FREE(vm, Value, array->values); // Dispose of the old array buffer
    array->length = length;
    array->values = values; // Assign the new buffer


  *result = receiver;
  return true;
}


// Hard-coded properties of ObjArray type
// TODO: Replace the multiple calls to strncpy() with something more efficient
bool arrayProperty(void* vm, Value receiver, ObjString* name) {
  ObjArray* array = AS_ARRAY(receiver);
  Value result;
  if (strncmp(name->chars, "length", name->length)==0) {
    result = NUMBER_VAL(array->length);
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "shift", name->length)==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, array_shift));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "unshift", name->length)==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, array_unshift));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "pop", name->length)==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, array_pop));
    pop(vm);
    push(vm, result);
    return true;
  }
  if (strncmp(name->chars, "push", name->length)==0) {
    result = OBJ_VAL(newNativeMethod(vm, receiver, array_push));
    pop(vm);
    push(vm, result);
    return true;
  }
  runtimeError(vm, "Undefined property '%s'.", name->chars);
  return false;
}

