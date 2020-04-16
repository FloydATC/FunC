#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"


void printObjectType(ObjType type) {
  switch (type) {
    case OBJ_BOUND_METHOD: printf("OBJ_BOUND_METHOD"); break;
    case OBJ_CLASS: printf("OBJ_CLASS"); break;
    case OBJ_CLOSURE: printf("OBJ_CLOSURE"); break;
    case OBJ_FUNCTION: printf("OBJ_FUNCTION"); break;
    case OBJ_INSTANCE: printf("OBJ_INSTANCE"); break;
    case OBJ_NATIVE: printf("OBJ_NATIVE"); break;
    case OBJ_NATIVE_METHOD: printf("OBJ_NATIVE_METHOD"); break;
    case OBJ_STRING: printf("OBJ_STRING"); break;
    case OBJ_ARRAY: printf("OBJ_ARRAY"); break;
    case OBJ_UPVALUE: printf("OBJ_UPVALUE"); break;
    default: printf("(unknown)"); break;
  }
}



#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocateObject(vm, sizeof(type), objectType)

static Obj* allocateObject(void* vm, size_t size, ObjType type) {
#ifdef DEBUG_LOG_GC
  printf("object:allocateObject() size=%d ", (uint32_t) size);
  printObjectType(type);
  printf("\n");
#endif

  Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
  object->type = type;
  object->isMarked = false;

  // Insert this object into the linked list so the GC can find it
  object->next = ((VM*)vm)->objects;
  ((VM*)vm)->objects = object;

//  printf("%p allocate %I64u for type=%d\n", (void*)object, size, type);

  return object;
}

ObjBoundMethod* newBoundMethod(void* vm, Value receiver, ObjClosure* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(vm, ObjBoundMethod, OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}


ObjClass* newClass(void* vm, ObjString* name) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newClass()\n");
#endif
  ObjClass* klass = ALLOCATE_OBJ(vm, ObjClass, OBJ_CLASS);
  klass->name = name;
  initTable(&klass->methods);
  return klass;
}


ObjClosure* newClosure(void* vm, ObjFunction* function) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newClosure()\n");
#endif
  ObjUpvalue** upvalues = ALLOCATE(vm, ObjUpvalue*, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure* closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}


ObjFunction* newFunction(void* vm) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newFunction()\n");
#endif
  ObjFunction* function = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);

  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newFunction() initializing chunk %p\n", &function->chunk);
#endif
  initChunk(vm, &function->chunk);
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newFunction() created function %p\n", function);
#endif
  return function;
}


ObjInstance* newInstance(void* vm, ObjClass* klass) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newInstance()\n");
#endif
  ObjInstance* instance = ALLOCATE_OBJ(vm, ObjInstance, OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}


ObjNative* newNative(void* vm, ObjString* name, NativeFn function) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newNative()\n");
#endif
  ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
  native->name = name;
  native->function = function;
  return native;
}


ObjNativeMethod* newNativeMethod(void* vm, Value receiver, ObjString* name, NativeMFn function) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newNativeMethod()\n");
#endif
  ObjNativeMethod* native = ALLOCATE_OBJ(vm, ObjNativeMethod, OBJ_NATIVE_METHOD);
  native->receiver = receiver;
  native->name = name;
  native->function = function;
  return native;
}


ObjArray* newArray(void* vm) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newArray()\n");
#endif
  ObjArray* array = ALLOCATE_OBJ(vm, ObjArray, OBJ_ARRAY);
  array->length = 0;
  array->values = NULL;
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newArray() allocated empty array %p\n", array);
#endif
  return array;
}

ObjArray* newArrayZeroed(void* vm, int length) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newArray()\n");
#endif
  ObjArray* array = ALLOCATE_OBJ(vm, ObjArray, OBJ_ARRAY);
  array->length = 0;
  push(vm, OBJ_VAL(array));
  array->values = ALLOCATE(vm, Value, length);
  for (int i=0; i<length; i++) array->values[i] = NULL_VAL;
  pop(vm);
  array->length = length;
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newArray() allocated empty array %p\n", array);
#endif
  return array;
}

void loadArray(void* vm, ObjArray* array, Value* values, int length) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:loadArray() loading from stack array=%p values=%p length=%d\n", array, values, length);
#endif
  if (length>0) {
    array->values = ALLOCATE(vm, Value, length);
    array->length = length;
    memcpy(array->values, values, length * sizeof(Value));
  }
#ifdef DEBUG_TRACE_OBJECTS
  for (int i=0; i<length; i++) {
    printf("  %4d: ", i);
    printValue(array->values[i]);
    printf("\n");
  }
  printf("object:loadArray() finished loading array=%p values=%p length=%d\n", array, values, length);
#endif

}


static ObjString* allocateString(void* vm, char* chars, int length, uint32_t hash) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:allocateString() chars=%.*s, length=%d\n", length, chars, length);
#endif
  ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(vm, OBJ_VAL(string)); // tableSet() may trigger GC
  tableSet(vm, &((VM*)vm)->strings, string, NULL_VAL);
  pop(vm);

#ifdef DEBUG_TRACE_OBJECTS
  printf("object:allocateString() returning %p\n", string);
#endif
  return string;
}


static uint32_t hashString(const char* key, int length) {
  // Implements the FNV-1a hash algorithm.
  // Simple. Fast. Great for tables, useless for crypto.
  uint32_t hash = 2166136261u;

  for (int i = 0; i < length; i++) {
    hash ^= key[i]; // Bitwise XOR
    hash *= 16777619;
  }

  return hash;
}


ObjString* takeString(void* vm, char* chars, int length) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:takeString() chars=%.*s, length=%d\n", length, chars, length);
#endif
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&((VM*)vm)->strings, chars, length, hash);
  if (interned != NULL) {
    //printf("object.takeString() duplicate, freeing %p\n", (void*)chars);
    FREE_ARRAY(vm, char, chars, length + 1);
    return interned; // We already have this string internally
  }
  return allocateString(vm, chars, length, hash);
}


// Use this for copying strings from the source code
// or from native functions via to_stringValue() in vm.c
ObjString* copyString(void* vm, const char* chars, int length) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:copyString() chars=%.*s, length=%d\n", length, chars, length);
#endif
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&((VM*)vm)->strings, chars, length, hash);
  if (interned != NULL) return interned; // We already have this string internally

  char* heapChars = ALLOCATE(vm, char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';

  return allocateString(vm, heapChars, length, hash);
}


ObjUpvalue* newUpvalue(void* vm, Value* slot) {
#ifdef DEBUG_TRACE_OBJECTS
  printf("object:newUpvalue() slot=%p\n", slot);
#endif
  ObjUpvalue* upvalue = ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NULL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}


static void printFunction(ObjFunction* function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<%s()>", function->name->chars);
}


void printObject(Value value) {
  //printf("object:printObject() type=%d\n", OBJ_TYPE(value));
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD:
      printFunction(AS_BOUND_METHOD(value)->method->function);
      break;
    case OBJ_CLASS:
      // TODO: some sort of toString() call?
      printf("<%s().>", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
    case OBJ_INSTANCE:
      // TODO: some sort of toString() call?
      printf("<%s.>", AS_INSTANCE(value)->klass->name->chars);
      break;
    case OBJ_NATIVE:
      printf("<%s()>", (AS_NATIVE(value)->name->chars));
      break;
    case OBJ_NATIVE_METHOD:
      printf("<.%s()>", (AS_NATIVE_METHOD(value)->name->chars));
      break;
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
    case OBJ_UPVALUE:
      printf("<upvalue>");
      break;
    case OBJ_ARRAY:
      printf("[array:%d]", AS_ARRAY(value)->length);
      break;
  }
}

// Object type names from USER perspective
char* getObjectTypeAsString(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_CLASS: return "class";
    case OBJ_INSTANCE: return "instance";
    case OBJ_ARRAY: return "array";
    case OBJ_NATIVE:
    case OBJ_NATIVE_METHOD:
    case OBJ_BOUND_METHOD:
    case OBJ_CLOSURE: return "function";
    case OBJ_STRING: return "string";
    default: return "internal"; // Not actually user visible
  }
}

// Return user visible types for each type of object Value
// This refers to built-in types, not user defined classes/instances
// Note: If it acts like a function to the user, return "function"
Value getObjectTypeAsValue(void* vm, Value value) {
  char* type = getObjectTypeAsString(value);
  return OBJ_VAL(copyString(vm, type, strlen(type)));
}


// Comparing certain objects with > >= < <= allows for sorting
// Note: Called via value:valuesGreater() for Obj values
// Future enhancement: Substitute strcmp() for something that understands UTF8 and locales
bool objectsGreater(Obj* a, Obj* b) {
  if (a->type != b->type) return false; // Dissimilar types have no sort order
  switch (a->type) {
    case OBJ_ARRAY: return ((ObjArray*)a)->length > ((ObjArray*)b)->length; // Sort arrays by length
    case OBJ_STRING: return (strcmp(((ObjString*)a)->chars, ((ObjString*)b)->chars) > 0); // Sort strings using strcmp()
    default: return false; // Other objects have no defined sort order
  }
}

