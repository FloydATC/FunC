#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "value.h"
#include "object.h"
#include "vm.h"

void initValueArray(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(void* vm, ValueArray* array, Value value) {
#ifdef DEBUG_TRACE_VALUES
  printf("value:writeValueArray() array=%p value=", array);
  printValueType(value.type);
  printf(":");
  printValue(value);
  printf("\n");
#endif

  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(vm, array->values, Value,
                               oldCapacity, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(void* vm, ValueArray* array) {
  FREE_ARRAY(vm, Value, array->values, array->capacity);
  initValueArray(array);
}

void printValue(Value value) {
  switch (value.type) {
    case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
    case VAL_NULL:   printf("null"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJ:    printObject(value); break;
  }
}

void printValueType(ValueType type) {
  switch (type) {
    case VAL_BOOL:   printf("VAL_BOOL"); break;
    case VAL_NULL:   printf("VAL_NULL"); break;
    case VAL_NUMBER: printf("VAL_NUMBER"); break;
    case VAL_OBJ:    printf("VAL_OBJ"); break;
    default:         printf("(unknown)");
  }
}

bool valuesEqual(Value a, Value b) {
  if (a.type != b.type) return false;

  switch (a.type) {
    case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NULL:    return true; // If we made it here, both are NULL
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    // All string objects are unique
    // This means we don't have to compare each character
    case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
  }
  return false;
}

bool valuesGreater(Value a, Value b) {
  if (a.type != b.type) return false;
  // Return true if a is greater than b

  switch (a.type) {
    case VAL_BOOL:   return false; // Booleans have no sense of less/greater
    case VAL_NULL:    return false; // Null has no value
    case VAL_NUMBER: return AS_NUMBER(a) > AS_NUMBER(b);
    // All string objects are unique
    // This means we don't have to compare each character
    case VAL_OBJ:    return objectsGreater(AS_OBJ(a), AS_OBJ(b));
  }
  return false;
}

// Return user visible types for each type of object Value
char* getValueTypeString(Value value) {
  switch (value.type) {
    case VAL_BOOL: return "boolean";
    case VAL_NULL: return "null";
    case VAL_NUMBER: return "number";
    case VAL_OBJ: return getObjectTypeString(value);
    default: return "invalid";
  }
}

// Return user visible types for each type of object Value as a Value
Value getValueType(void* vm, Value value) {
  switch (value.type) {
    case VAL_BOOL: return OBJ_VAL(copyString(vm, "boolean", 7));
    case VAL_NULL: return OBJ_VAL(copyString(vm, "null", 4));
    case VAL_NUMBER: return OBJ_VAL(copyString(vm, "number", 6));
    case VAL_OBJ: return getObjectType(vm, value);
    default: return OBJ_VAL(copyString(vm, "invalid", 7));
  }
}

