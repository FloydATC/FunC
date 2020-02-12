#ifndef func_object_h
#define func_object_h

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)

#define IS_CLASS(value)         isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)       isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)      isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)      isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)        isObjType(value, OBJ_NATIVE)
#define IS_NATIVEMETHOD(value)  isObjType(value, OBJ_NATIVEMETHOD)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)
#define IS_ARRAY(value)         isObjType(value, OBJ_ARRAY)

#define AS_CLASS(value)         ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)      ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)        (((ObjNative*)AS_OBJ(value))->function)
#define AS_NATIVEMETHOD(value)  ((ObjNativeMethod*)AS_OBJ(value))
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))
#define AS_ARRAY(value)         ((ObjArray*)AS_OBJ(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJ(value))->chars)



typedef enum {
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_INSTANCE,
  OBJ_NATIVE,
  OBJ_NATIVEMETHOD,
  OBJ_STRING,
  OBJ_ARRAY,
  OBJ_UPVALUE,
} ObjType;

struct sObj {
  ObjType type;
  bool isMarked; // GC: In use
  struct sObj* next;
};

typedef struct {
  Obj obj;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

typedef bool (*NativeFn)(void* vm, int argCount, Value* args, Value* result);
typedef bool (*NativeMFn)(void* vm, Value receiver, int argCount, Value* args, Value* result);
typedef void (*ErrorCb)(const char* format,...);

typedef struct {
  Obj obj;
  NativeFn function;
} ObjNative;

typedef struct {
  Obj obj;
  Value receiver;
  NativeMFn function;
} ObjNativeMethod;

struct sObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash; // Note: strings must be immutable OR this must invalidate/recalc
};

typedef struct {
  Obj obj;
  int length;
  Value* values;
} ObjArray; // An array holds a malloc'ed buffer of Values

typedef struct sUpvalue {
  Obj obj;
  Value* location;
  Value closed;
  struct sUpvalue* next;
} ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;

typedef struct sObjClass {
  Obj obj;
  ObjString* name;
} ObjClass;

typedef struct {
  Obj obj;
  ObjClass* klass;
  Table fields;
} ObjInstance; // Instance of an ObjClass

ObjClass* newClass(void* vm, ObjString* name);
ObjClosure* newClosure(void* vm, ObjFunction* function);
ObjFunction* newFunction(void* vm);
ObjInstance* newInstance(void* vm, ObjClass* klass);
ObjNative* newNative(void* vm, NativeFn function);
ObjNativeMethod* newNativeMethod(void* vm, Value receiver, NativeMFn function);
ObjArray* newArray(void* vm);
void loadArray(void* vm, ObjArray* array, Value* values, int length);
ObjString* takeString(void* vm, char* chars, int length);
ObjString* copyString(void* vm, const char* chars, int length);
ObjUpvalue* newUpvalue(void* vm, Value* slot);
Value getObjectType(void* vm, Value value);
void printObject(Value value);
void printObjectType(ObjType type);
bool objectsGreater(Obj* a, Obj* b);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}


#endif

