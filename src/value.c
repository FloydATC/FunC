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
#ifdef NAN_BOXING
  if (IS_BOOL(value)) {
    printf(AS_BOOL(value) ? "true" : "false");
  } else if (IS_NULL(value)) {
    printf("null");
  } else if (IS_NUMBER(value)) {
    printf("%g", AS_NUMBER(value));
  } else if (IS_OBJ(value)) {
    printObject(value);
  }                                          
#else
  switch (value.type) {
    case VAL_BOOL:   printf(AS_BOOL(value) ? "true" : "false"); break;
    case VAL_NULL:   printf("null"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJ:    printObject(value); break;
  }
#endif
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
#ifdef NAN_BOXING
  if (IS_NUMBER(a) && IS_NUMBER(b)) return AS_NUMBER(a) == AS_NUMBER(b); // Satisfy NaN != NaN
  return a == b;
#else
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
#endif
}

bool valuesGreater(Value a, Value b) {
  if (IS_NUMBER(a) && IS_NUMBER(b)) return AS_NUMBER(a) > AS_NUMBER(b);
  if (IS_OBJ(a) && IS_OBJ(b)) return objectsGreater(AS_OBJ(a), AS_OBJ(b));
  return false;
}

// Return user visible types for each type of object Value
char* getTypeAsString(Value value) {
#ifdef NAN_BOXING
  if (IS_BOOL(value)) {
    return "boolean";
  } else if (IS_NULL(value)) {
    return "null";
  } else if (IS_NUMBER(value)) {
    return "number";
  } else if (IS_OBJ(value)) {
    return getObjectTypeAsString(value);
  } else {
    return "invalid";
  }
#else
  switch (value.type) {
    case VAL_BOOL: return "boolean";
    case VAL_NULL: return "null";
    case VAL_NUMBER: return "number";
    case VAL_OBJ: return getObjectTypeAsString(value);
    default: return "invalid";
  }
#endif
}

// Return user visible types for each type of object Value as a Value
Value getTypeAsValue(void* vm, Value value) {
  char* type = getTypeAsString(value);
  return OBJ_VAL(copyString(vm, type, (int)strlen(type)));
}

