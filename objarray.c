#include <string.h>

#include "objarray.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#define CHECK_ARG_IS_STRING(index) \
  if (argCount >= index+1 && !IS_STRING(args[index])) { \
    runtimeError(vm, "Argument %d must be a string, got %s.", index+1, getValueTypeString(args[index])); \
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



// Utility method: recursively count elements in nested array
int count_nested_array_elements(ObjArray* array) {
  int length = 0;
  for (int i=0; i<array->length; i++) {
    if (IS_ARRAY(array->values[i])) {
      length += count_nested_array_elements(AS_ARRAY(array->values[i]));
    } else {
      length++;
    }
  }
  return length;
}

// Utility method: recursively count elements in nested array
int copy_nested_array_elements(ObjArray* array, Value* buf, int offset) {
  for (int i=0; i<array->length; i++) {
    if (IS_ARRAY(array->values[i])) {
      offset = copy_nested_array_elements(AS_ARRAY(array->values[i]), buf, offset);
    } else {
      buf[offset++] = array->values[i];
    }
  }
  return offset;
}



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

// Native C method: ARRAY.fill()
static bool array_fill(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1) {
    runtimeError(vm, "Method needs 1 argument.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);

  for (int i=0; i<array->length; i++) {
    array->values[i] = args[0];
  }

  *result = receiver;
  return true;
}

// Native C method: ARRAY.size()
static bool array_size(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1) {
    runtimeError(vm, "Method takes 1 argument.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);

  int length = array->length;
  int newlen = AS_NUMBER(args[0]);

  if (newlen != length) {
    // Resize array
    array->length = newlen;
    array->values = reallocate(vm, array->values, length*sizeof(Value), newlen*sizeof(Value));
  }
  if (newlen > length) {
    // Size increased, initialize new elements to NULL
    for (int i=length; i<newlen; i++) {
      array->values[i] = NULL_VAL;
    }
  }

  *result = receiver;
  return true;
}


// Native C method: ARRAY.flat()
static bool array_flat(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  (unused)args;
  if (argCount != 0) {
    runtimeError(vm, "Method does not take any arguments.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);

  // Recursively scan nested arrays and summarize the elements
  int length = count_nested_array_elements(array);

  // Create a new array
  ObjArray* res = newArray(vm);
  res->values = ALLOCATE(vm, Value, length);
  res->length = length;

  // Recursively copy nested arrays elements into buffer
  copy_nested_array_elements(array, res->values, 0);

  *result = OBJ_VAL(res);
  return true;
}


// Native C method: ARRAY.join()
static bool array_join(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  if (argCount != 1 || !IS_STRING(args[0])) {
    runtimeError(vm, "Method takes 1 string argument.");
    return false;
  }
  ObjArray* array = AS_ARRAY(receiver);

  // Edge case: Joining an array with 0 elements returns empty string
  if (array->length == 0) {
    *result = OBJ_VAL(copyString(vm, "", 0));
    return true;
  }

  // Edge case: Joining an array with 1 element returns that element
  if (array->length == 1 && IS_STRING(array->values[0])) {
    *result = array->values[0];
    return true;
  }

  // Get size of the string buffer needed, this also verifies all elements are strings
  ObjString* separator = AS_STRING(args[0]);
  int length = (array->length-1) * separator->length;
  for (int i=0; i<array->length; i++) {
    if (IS_STRING(array->values[i])) {
      length += AS_STRING(array->values[i])->length;
    } else {
      runtimeError(vm, "Can not join() non-string array element.");
      return false;
    }
  }

  // Allocate the string buffer
  char* buf = ALLOCATE(vm, char, length + 1);

  // Copy strings into buffer
  int offset = 0;
  for (int i=0; i<array->length; i++) {
    // Copy element
    ObjString* element = AS_STRING(array->values[i]);
    strncpy(buf+offset, element->chars, element->length);
    offset += element->length;
    // Copy separator unless we are at the last element
    if (i < array->length-1) {
      strncpy(buf+offset, separator->chars, separator->length);
      offset += separator->length;
    }
  }
  buf[length] = '\0'; // Terminate

  // Finally, hand the new buffer over
  *result = OBJ_VAL(takeString(vm, buf, length));
  return true;
}

#define METHOD(fn_name, fn_call) \
  if (strcmp(name->chars, fn_name)==0) { \
    result = OBJ_VAL(newNativeMethod(vm, receiver, fn_call)); \
    pop(vm); \
    push(vm, result); \
    return true; \
  }

// Hard-coded properties of ObjArray type
// TODO: Replace the multiple calls to strncpy() with something more efficient
bool arrayProperty(void* vm, Value receiver, ObjString* name) {
  ObjArray* array = AS_ARRAY(receiver);
  Value result;
  if (strcmp(name->chars, "length")==0) {
    result = NUMBER_VAL(array->length);
    pop(vm);
    push(vm, result);
    return true;
  }

  METHOD("shift",   array_shift);
  METHOD("unshift", array_unshift);
  METHOD("pop",     array_pop);
  METHOD("push",    array_push);
  METHOD("fill",    array_fill);
  METHOD("size",    array_size);
  METHOD("flat",    array_flat);
  METHOD("join",    array_join);

  runtimeError(vm, "Array has no '%s'.", name->chars);
  return false;
}

#undef METHOD

#undef CHECK_ARG_IS_STRING
#undef CHECK_ARGS_ZERO_OR_ONE
#undef CHECK_ARGS_ONE_OR_TWO
#undef CHECK_ARGS_ONE

