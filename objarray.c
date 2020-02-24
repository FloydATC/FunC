#include <string.h>
#include <stdio.h>

#include "objarray.h"
#include "memory.h"
#include "number.h"
#include "object.h"
#include "vm.h"

#define CHECK_ARG_IS_STRING(index) \
  if (argCount >= index+1 && !IS_STRING(args[index])) { \
    runtimeError(vm, "Argument %d must be a string, got %s.", index+1, getValueTypeString(args[index])); \
    return false; \
  }

#define CHECK_ARG_IS_ARRAY(index) \
  if (argCount >= index+1 && !IS_ARRAY(args[index])) { \
    runtimeError(vm, "Argument %d must be an array, got %s.", index+1, getValueTypeString(args[index])); \
    return false; \
  }

#define CHECK_ARGS_ZERO() \
  if (argCount > 0) { \
    runtimeError(vm, "Method takes no arguments, got %d.", argCount); \
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

#define CHECK_ARGS_ONE_OR_MORE() \
  if (argCount < 1) { \
    runtimeError(vm, "Method takes one or more argument, got %d.", argCount); \
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
  CHECK_ARGS_ZERO();

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
  CHECK_ARGS_ONE_OR_MORE();

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
  CHECK_ARGS_ZERO();

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
  CHECK_ARGS_ONE_OR_MORE();

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
  CHECK_ARGS_ONE();

  ObjArray* array = AS_ARRAY(receiver);

  for (int i=0; i<array->length; i++) {
    array->values[i] = args[0];
  }

  *result = receiver;
  return true;
}

// Native C method: ARRAY.resize()
static bool array_resize(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();

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
  CHECK_ARGS_ZERO();

  ObjArray* array = AS_ARRAY(receiver);

  // Recursively scan nested arrays and summarize the elements
  int length = count_nested_array_elements(array);

  // Create a new array
  ObjArray* res = newArray(vm);
  push(vm, OBJ_VAL(res)); // Protect from GC
  res->values = ALLOCATE(vm, Value, length);
  pop(vm);
  res->length = length;

  // Recursively copy nested arrays elements into buffer
  copy_nested_array_elements(array, res->values, 0);

  *result = OBJ_VAL(res);
  return true;
}


// Native C method: ARRAY.join()
static bool array_join(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();
  CHECK_ARG_IS_STRING(0);

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
  if (array->length == 1 && IS_NUMBER(array->values[0])) {
    char* buf;
    int length = double_to_str_dec(AS_NUMBER(array->values[0]), &buf);
    *result = OBJ_VAL(takeString(vm, buf, length));
    return true;
  }

  // Get size of the string buffer needed, this also verifies all elements are strings
  ObjString* separator = AS_STRING(args[0]);
  int length = (array->length-1) * separator->length;
  for (int i=0; i<array->length; i++) {
    if (IS_STRING(array->values[i])) {
      length += AS_STRING(array->values[i])->length;
    } else if (IS_NUMBER(array->values[i])) {
      char* buf;
      int strlen = double_to_str_dec(AS_NUMBER(array->values[i]), &buf);
      takeString(vm, buf, strlen);
      length += strlen;
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
    ObjString* element;
    if (IS_STRING(array->values[i])) {
      element = AS_STRING(array->values[i]);
    } else if (IS_NUMBER(array->values[i])) {
      char* buf;
      int strlen = double_to_str_dec(AS_NUMBER(array->values[i]), &buf);
      element = takeString(vm, buf, strlen);
    }
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





// Native C method: ARRAY.mul4() -- ROW MAJOR matrix multiplication with 4 rows
static bool array_mul4(void* vm, Value receiver, int argCount, Value* args, Value* result) {
  CHECK_ARGS_ONE();
  CHECK_ARG_IS_ARRAY(0);

  ObjArray* m1 = AS_ARRAY(receiver);
  ObjArray* m2 = AS_ARRAY(args[0]);

  int size = 4;

  if (m1->length == 0 || m1->length % size != 0) {
    runtimeError(vm, "Array 1 not multiple of %d.", size);
    return false;
  }
  if (m2->length == 0 || m2->length % size != 0) {
    runtimeError(vm, "Array 2 not multiple of %d.", size);
    return false;
  }

  int m1rows = m1->length / size;
  int m1cols = size;
  //int m2rows = size;
  int m2cols = m2->length / size;

  // Create output array
  ObjArray* product = newArray(vm);
  push(vm, OBJ_VAL(product)); // Protect from GC
  product->values = ALLOCATE(vm, Value, m1rows*m2cols);
  pop(vm);
  //printf("objarray:array_mul4() product->values:\n");
  //hexdump(product->values, 16*sizeof(Value));
  product->length = m1rows*m2cols;
  //printf("objarray:array_mul4() product->values=%p rows=%d cols=%d length=%d\n", product->values, m1rows, m2cols, product->length);
  //printf("objarray:array_mul4() sizeof(Value)=%d\n", (int) sizeof(Value));

  //printf("objarray:array_mul4() product->values:\n");
  //hexdump(product->values, 16*sizeof(Value));

  // Multiply
  for (int i=0; i<m1rows; i++) {
    for (int j=0; j<m2cols; j++) {
      double sum = 0;
      for (int k=0; k<m1cols; k++) {
        //printf("objarray:array_mul4() sum += m1[%d,%d] %f * m2[%d,%d] %f\n", i, k, AS_NUMBER(m1->values[(i*m1cols)+k]), k, j, AS_NUMBER(m2->values[(k*m2cols)+j]));
        sum += AS_NUMBER(m1->values[(i*m1cols)+k]) * AS_NUMBER(m2->values[(k*m2cols)+j]);
      }
      //int offset = (i*m1rows)+j;
      //printf("objarray:array_mul4() row=%d*%d col=%d sum=%f\n", i, m2cols, j, sum);
      //hexdump(product->values, 16*sizeof(Value));
      product->values[(i*m2cols)+j] = NUMBER_VAL(sum);
    }
  }

  *result = OBJ_VAL(product);
  return true;
}



#define METHOD(fn_name, fn_call) \
  if (strcmp(name->chars, fn_name)==0) { \
    *property = OBJ_VAL(newNativeMethod(vm, receiver, name, fn_call)); \
    return true; \
  }


// Hard-coded properties of ObjArray type
// TODO: Replace the multiple calls to strncpy() with something more efficient
bool getArrayProperty(void* vm, Value receiver, ObjString* name, Value* property) {
  ObjArray* array = AS_ARRAY(receiver);

  if (strcmp(name->chars, "length")==0) {
    *property = NUMBER_VAL(array->length);
    return true;
  }

  METHOD("shift",   array_shift);
  METHOD("unshift", array_unshift);
  METHOD("pop",     array_pop);
  METHOD("push",    array_push);
  METHOD("fill",    array_fill);
  METHOD("resize",  array_resize);
  METHOD("flat",    array_flat);
  METHOD("join",    array_join);

  METHOD("mul4",    array_mul4);

  runtimeError(vm, "Array has no '%s'.", name->chars);
  return false;
}


// Get an array property, pop receiver and push the property
bool pushArrayProperty(void* vm, Value receiver, ObjString* name) {
  Value method;
  if (!getArrayProperty(vm, receiver, name, &method)) return false;
  pop(vm); // receiver
  push(vm, method);
  return true;
}



#undef METHOD

#undef CHECK_ARG_IS_STRING
#undef CHECK_ARGS_ZERO_OR_ONE
#undef CHECK_ARGS_ONE_OR_TWO
#undef CHECK_ARGS_ONE

