#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#if defined(DEBUG_LOG_GC) || defined(DEBUG_LOG_GC_VERBOSE) || defined(DEBUG_TRACE_MEMORY)
#include "debug.h"
#endif


#define GC_HEAP_GROW_FACTOR 2

//struct VM;

void hexdump(const void* data, size_t size) {
  uint16_t addr = (unsigned long long int) data & 0xffff;
  char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
    if (i % 16 == 0) printf("%04X  ", (uint16_t) (addr + (uint16_t) i));
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}


void* reallocate(void* vm, void* previous, size_t oldSize, size_t newSize) {
  ((VM*)vm)->bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif
    if (((VM*)vm)->bytesAllocated > ((VM*)vm)->nextGC) {
      collectGarbage(vm);
    }
  }
  if (newSize == 0) {
#ifdef DEBUG_TRACE_MEMORY
    printf("memory:reallocate() free(%p) (%d bytes)\n", previous, (int)oldSize);
    if (previous != NULL) hexdump(previous, oldSize);
#endif
    free(previous);
#ifdef DEBUG_TRACE_MEMORY
    printf("memory:reallocate() freed ok\n");
#endif
    return NULL;
  }

#ifdef DEBUG_TRACE_MEMORY
  printf("memory:reallocate() realloc(%p, %d) (%d -> %d)\n", previous, (int)newSize, (int)oldSize, (int)newSize);
#ifdef DEBUG_TRACE_MEMORY_HEXDUMP
  if (oldSize > 0) hexdump(previous, oldSize);
#endif
#endif
  uint8_t* ptr = realloc(previous, newSize);
#ifdef DEBUG_TRACE_MEMORY
  printf("memory:reallocate() allocated %p\n", ptr);
#endif
  if (ptr==NULL) printf("WARNING! memory:reallocate() realloc() returned null pointer!\n");

  // Set all NEW memory to sentinel value \xAA
  if (oldSize < newSize) memset(ptr + oldSize, 0xAA, newSize-oldSize);

  return ptr;
}

void markObject(void* vm, Obj* object) {
  if (object == NULL) return;
  if (object->isMarked) return; // Already checked this Obj
#ifdef DEBUG_LOG_GC_VERBOSE
  printf("memory:markObject(vm=%p, object=%p) ", vm, (void*)object);
  printObjectType(object->type);
  printf(" ");
  printValue(OBJ_VAL(object));
  printf("\n");
#ifdef DEBUG_LOG_GC_HEXDUMP
  if (object->type == OBJ_FUNCTION) hexdump(object, sizeof(ObjFunction));
#endif
#endif

  if (object->type == OBJ_ARRAY) {
    ObjArray* array = (ObjArray*)object; // Userspace object.h:ObjArray
    for (int i = 0; i < array->length; i++) {
      //printf("EXPERIMENTAL: markObject ObjArray %p element i=%d %p\n", array, i, array->values+i);
      markValue(vm, array->values[i]); // EXPERIMENTAL!!!
    }
  }

  object->isMarked = true;

#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markObject() vm %p greystack capacity=%d count=%d\n", vm, ((VM*)vm)->grayCapacity, ((VM*)vm)->grayCount);
#endif
  if (((VM*)vm)->grayCapacity < ((VM*)vm)->grayCount + 1) {
#ifdef DEBUG_LOG_GC_VERBOSE
    printf("memory:markObject() extending graystack\n");
#endif
    ((VM*)vm)->grayCapacity = GROW_CAPACITY(((VM*)vm)->grayCapacity);
    ((VM*)vm)->grayStack = realloc(((VM*)vm)->grayStack,
                           sizeof(Obj*) * ((VM*)vm)->grayCapacity);
  }

#ifdef DEBUG_LOG_GC_EXTREME
    printf("memory:markObject() placing object in graystack %p slot %d\n", ((VM*)vm)->grayStack, ((VM*)vm)->grayCount);
#endif
  ((VM*)vm)->grayStack[((VM*)vm)->grayCount++] = object;
#ifdef DEBUG_LOG_GC_EXTREME
    printf("memory:markObject() marked %p\n", object);
#endif
}

void markValue(void* vm, Value value) {
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markValue(vm=%p, value=", vm);
  printValueType(value.type);
  printf(":");
  printValue(value);
  printf("\n");
#endif
  if (!IS_OBJ(value)) return;
  markObject(vm, AS_OBJ(value));
}

// Internal value.h:ValueArray, not userspace object.h:ObjArray
static void markArray(void* vm, ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markValue(vm, array->values[i]);
  }
}

static void blackenObject(void* vm, Obj* object) {
#ifdef DEBUG_LOG_GC_VERBOSE
  printf("%p blacken ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      markValue(vm, bound->receiver);
      markObject(vm, (Obj*)bound->method); // Not really needed, but best practice
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      markObject(vm, (Obj*)klass->name);
      markTable(vm, &klass->methods);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      markObject(vm, (Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        markObject(vm, (Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      markObject(vm, (Obj*)function->name);
      markArray(vm, &function->chunk.constants); // Internal value.h:ValueArray

      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      markObject(vm, (Obj*)instance->klass);
      markTable(vm, &instance->fields);
      break;
    }
    case OBJ_UPVALUE:
      markValue(vm, ((ObjUpvalue*)object)->closed);
      break;
    case OBJ_NATIVE_METHOD:
      markValue(vm, ((ObjNativeMethod*)object)->receiver);
      break;
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
    case OBJ_ARRAY: {
      ObjArray* array = (ObjArray*)object; // Userspace object.h:ObjArray
      for (int i = 0; i < array->length; i++) {
        markValue(vm, array->values[i]); // EXPERIMENTAL!!!
      }
      break;
    }
  }
}

static void freeObject(void* vm, Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("memory:freeObject(%p) type=%d ", (void*)object, object->type);
  printObjectType(object->type);
  printf ("\n");
#ifdef DEBUG_TRACE_MEMORY_HEXDUMP
  hexdump(object, sizeof(Obj));
#endif
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      // A bound method does not own any of its contents, it only references them
      FREE(vm, ObjBoundMethod, object);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      freeTable(vm, &klass->methods);
      FREE(vm, ObjClass, object);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(vm, ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(vm, ObjClosure, object);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(vm, &function->chunk);
      FREE(vm, ObjFunction, object);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      freeTable(vm, &instance->fields);
      FREE(vm, ObjInstance, object);
      break;
    }
    case OBJ_NATIVE: {
      FREE(vm, ObjNative, object);
      break;
    }
    case OBJ_NATIVE_METHOD: {
      FREE(vm, ObjNativeMethod, object);
      break;
    }
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;
      FREE_ARRAY(vm, char, string->chars, string->length + 1);
      FREE(vm, ObjString, object);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(vm, ObjUpvalue, object);
      break;
    }
    case OBJ_ARRAY: {
      ObjArray* array = (ObjArray*)object;
      FREE_ARRAY(vm, Value, array->values, array->length);
      FREE(vm, ObjArray, array);
      break;
    }
  }
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
  printf("memory:freeObject(%p) done\n", (void*)object);
#endif // DEBUG_TRACE_MEMORY_VERBOSE
}

static void markRoots(VM* vm) {
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markRoots(vm=%p) scan locals\n", vm);
#endif
  // Scan local variables
  for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
    markValue(vm, *slot);
  }

  // Scan CallFrame closures for constants and upvalues
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markRoots(vm=%p) scan callframes\n", vm);
#endif
  for (int i = 0; i < vm->frameCount; i++) {
    markObject(vm, (Obj*)vm->frames[i].closure);
  }

  // Scan list of open upvalues
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markRoots(vm=%p) scan upvalues\n", vm);
#endif
  for (ObjUpvalue* upvalue = vm->openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    markObject(vm, (Obj*)upvalue);
  }

  // Scan global variables
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markRoots(vm=%p) scan globals\n", vm);
#endif
  markTable(vm, &vm->globals);

  // Scan filename array -- Experimental include support
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markRoots(vm=%p) scan filenames\n", vm);
#endif
  markArray(vm, &vm->filenames);

  // Scan objects in use at compile time
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markRoots(vm=%p) scan compiler\n", vm);
#endif
  markCompilerRoots(vm);
#ifdef DEBUG_LOG_GC_EXTREME
  printf("memory:markRoots(vm=%p) include the initString\n", vm);
#endif
  markObject(vm, (Obj*)vm->initString);
}

static void traceReferences(void* vm) {
  while (((VM*)vm)->grayCount > 0) {
    Obj* object = ((VM*)vm)->grayStack[--((VM*)vm)->grayCount];
    blackenObject(vm, object);
  }
}

static void sweep(void* vm) {
  Obj* previous = NULL;
  Obj* object = ((VM*)vm)->objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false; // Reset for next GC cycle
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;

      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        ((VM*)vm)->objects = object;
      }

      freeObject(vm, unreached);
    }
  }
}

void collectGarbage(VM* vm) {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm->bytesAllocated;
#endif

  markRoots(vm);
  traceReferences(vm); // Process the graystack
  tableRemoveWhite(vm, &((VM*)vm)->strings); // Process string interns
  sweep(vm);

  vm->nextGC = vm->bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %I64u bytes (from %I64u to %I64u) next at %I64u\n",
         before - vm->bytesAllocated, before, vm->bytesAllocated,
         vm->nextGC);
#endif
}

void freeObjects(VM* vm) {
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
  int total = 0;
  int count = 0;
#endif
  Obj* object = vm->objects;
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
  while (object != NULL) {
    total++;
    object = object->next;
  }
  object = vm->objects;
#endif
  while (object != NULL) {
#ifdef DEBUG_TRACE_MEMORY_VERBOSE
    count++;
    printf("memory:freeObjects() %d of %d:\n", count, total);
#endif
    Obj* next = object->next;
    freeObject(vm, object);
    object = next;
  }

#ifdef DEBUG_TRACE_MEMORY
  printf("memory:freeObjects() freeing %p...", vm->grayStack);
#endif
  free(vm->grayStack);
#ifdef DEBUG_TRACE_MEMORY
  printf("ok\n");
#endif
}


