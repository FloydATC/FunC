#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void dumpChunk(Chunk* chunk);
void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

// Not needed according to doc.
// Compiler barks if missing unless .c file declares them static.
// If declared static, the .c refuses to compile over "implicit declaration" something.
int simpleInstruction(const char* name, int offset);
int constantInstruction(const char* name, Chunk* chunk, int offset);
int invokeInstruction(const char* name, Chunk* chunk, int offset);
int byteInstruction(const char* name, Chunk* chunk, int offset);
int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset);
#endif
