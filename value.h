#ifndef clox_value_h
#define clox_value_h

#include "common.h"
//#include "vm.h"

//struct VM;

typedef struct sObj Obj;
typedef struct sObjString ObjString;

typedef enum {
  VAL_BOOL,
  VAL_NULL,
  VAL_NUMBER,
  VAL_OBJ,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Obj* obj;
  } as;
} Value;


// Type checking
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NULL(value)    ((value).type == VAL_NULL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

// From Value to native types: bool b = AS_BOOL(v)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_OBJ(value)     ((value).as.obj)

// From native types to Value: Value v = BOOL_VAL(true);
#define BOOL_VAL(value)   ((Value){ VAL_BOOL, { .boolean = value } })
#define NULL_VAL          ((Value){ VAL_NULL, { .number = 0 } })
#define NUMBER_VAL(value) ((Value){ VAL_NUMBER, { .number = value } })
#define OBJ_VAL(object)   ((Value){ VAL_OBJ, { .obj = (Obj*)object } })




typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(void* vm, ValueArray* array, Value value);
void freeValueArray(void* vm, ValueArray* array);
void printValue(Value value);
void printValueType(ValueType type);
Value getValueType(void* vm, Value value);



#endif
