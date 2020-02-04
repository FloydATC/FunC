#ifndef clox_memory_h
#define clox_memory_h

#include "object.h"
//#include "vm.h"

struct VM;

#define ALLOCATE(vm, type, count) \
    (type*)reallocate(vm, NULL, 0, sizeof(type) * (count))

#define FREE(vm, type, pointer) \
    reallocate(vm, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(vm, previous, type, oldCount, count) \
    (type*)reallocate(vm, previous, sizeof(type) * (oldCount), \
        sizeof(type) * (count))

#define FREE_ARRAY(vm, type, oldPointer, oldCount) \
    reallocate(vm, oldPointer, sizeof(type) * (oldCount), 0)


/*
oldSize 	newSize 	            Operation
0 	      Non-zero 	            Allocate new block.
Non-zero 	0	                    Free allocation.
Non-zero 	Smaller than oldSize 	Shrink existing allocation.
Non-zero 	Larger than oldSize 	Grow existing allocation.
*/
void* reallocate(void* vm, void* previous, size_t oldSize, size_t newSize);
void markObject(void* vm, Obj* object);
void markValue(void* vm, Value value);
void collectGarbage(); // Mark and sweep any Obj not in use
void freeObjects(); // Walk the VM's linked list of objects and free them all



#endif
