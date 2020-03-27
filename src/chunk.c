#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"
#include "vm.h" // push/pop

void initChunk(void* vm, Chunk* chunk) {
  (unused)vm;
#ifdef DEBUG_TRACE_CHUNK
  printf("chunk:initChunk(vm %p, chunk=%p)\n", vm, chunk);
#endif // DEBUG_TRACE_CHUNK
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->files = NULL;
  chunk->lines = NULL;
  chunk->chars = NULL;
#ifdef DEBUG_TRACE_CHUNK
  printf("chunk:initChunk() initializing constants array %p\n", &chunk->constants);
#endif // DEBUG_TRACE_CHUNK
  initValueArray(&chunk->constants);
#ifdef DEBUG_TRACE_CHUNK
  printf("chunk:initChunk(vm %p, chunk=%p) initialized ok\n", vm, chunk);
#endif // DEBUG_TRACE_CHUNK
}

void freeChunk(void* vm, Chunk* chunk) {
#ifdef DEBUG_TRACE_CHUNK
  printf("chunk:freeChunk(vm %p, chunk=%p)\n", vm, chunk);
#endif // DEBUG_TRACE_CHUNK
  FREE_ARRAY(vm, uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(vm, int, chunk->files, chunk->capacity);
  FREE_ARRAY(vm, int, chunk->lines, chunk->capacity);
  FREE_ARRAY(vm, int, chunk->chars, chunk->capacity);
  freeValueArray(vm, &chunk->constants);
#ifdef DEBUG_TRACE_CHUNK
  printf("chunk:freeChunk(vm %p, chunk=%p) freed ok\n", vm, chunk);
#endif // DEBUG_TRACE_CHUNK
  initChunk(vm, chunk);
}

void writeChunk(void* vm, Chunk* chunk, uint8_t byte, int fileno, int lineno, int charno) {
#ifdef DEBUG_TRACE_CHUNK
  printf("chunk:writeChunk(vm=%p, chunk=%p, byte=%d, file=%d, line=%d, char=%d)\n", vm, chunk, byte, fileno, lineno, charno);
#endif // DEBUG_TRACE_CHUNK
  if (chunk->capacity < chunk->count + 1) {
#ifdef DEBUG_TRACE_CHUNK
    printf("chunk:writeChunk() extending chunk:\n");
#endif // DEBUG_TRACE_CHUNK
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);      // memory.h:GROW_CAPACITY -> memory:reallocate()
#ifdef DEBUG_TRACE_CHUNK
    printf("chunk:writeChunk() extending array: code=%p\n", chunk->code);
#endif // DEBUG_TRACE_CHUNK
    chunk->code = GROW_ARRAY(vm, chunk->code, uint8_t, // memory:GROW_ARRAY -> memory:reallocate()
        oldCapacity, chunk->capacity);
#ifdef DEBUG_TRACE_CHUNK
    printf("chunk:writeChunk() extending array: files=%p\n", chunk->files);
#endif // DEBUG_TRACE_CHUNK
    chunk->files = GROW_ARRAY(vm, chunk->files, int,  // memory:GROW_ARRAY -> memory:reallocate()
        oldCapacity, chunk->capacity);
#ifdef DEBUG_TRACE_CHUNK
    printf("chunk:writeChunk() extending array: lines=%p\n", chunk->lines);
#endif // DEBUG_TRACE_CHUNK
    chunk->lines = GROW_ARRAY(vm, chunk->lines, int,  // memory.h:GROW_ARRAY -> memory:reallocate()
        oldCapacity, chunk->capacity);
#ifdef DEBUG_TRACE_CHUNK
    printf("chunk:writeChunk() extending array: chars=%p\n", chunk->chars);
#endif // DEBUG_TRACE_CHUNK
    chunk->chars = GROW_ARRAY(vm, chunk->chars, int,  // memory.h:GROW_ARRAY -> memory:reallocate()
        oldCapacity, chunk->capacity);
#ifdef DEBUG_TRACE_CHUNK
    printf("chunk:writeChunk() finished extending arrays for chunk %p\n", chunk);
#endif // DEBUG_TRACE_CHUNK
  }

  chunk->code[chunk->count] = byte;
  chunk->files[chunk->count] = fileno;
  chunk->lines[chunk->count] = lineno;
  chunk->chars[chunk->count] = charno;
  chunk->count++;
}

int addConstant(void* vm, Chunk* chunk, Value value) {
#ifdef DEBUG_TRACE_CHUNK
  printf("chunk.addConstant(%p, %p)\n", chunk, &value);
#endif // DEBUG_TRACE_CHUNK
  push(vm, value); // '() may trigger GC
  writeValueArray(vm, &chunk->constants, value);
  pop(vm);
  return chunk->constants.count - 1;
}

void writeConstant(void* vm, Chunk* chunk, Value value, int fileno, int lineno, int charno) {

#ifdef DEBUG_TRACE_CHUNK
  printf("chunk.writeConstant(%p, %p, %d, %d, %d)\n", chunk, &value, fileno, lineno, charno);
#endif // DEBUG_TRACE_CHUNK
  int constant = addConstant(vm, chunk, value); // Store in .constants
  writeChunk(vm, chunk, OP_CONSTANT, fileno, lineno, charno);
  writeChunk(vm, chunk, constant, fileno, lineno, charno);
}

