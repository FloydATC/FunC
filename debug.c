#include <stdio.h>

#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"



void disassembleChunk(Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);
  printf("%-8s %4s %4s %4s %4s %-16s\n", "chunk", "offs", "file", "line", "char", "instruction");

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%08x:%04x ", *((unsigned int*)chunk), offset);
  bool changed = false;
  if (offset > 0 && chunk->files[offset] == chunk->files[offset - 1]) {
    printf("   | ");
  } else {
    printf(" %4d ", chunk->files[offset]);
    changed = true;
  }
  if (changed==false && offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
    changed = true;
  }
  if (changed==false && offset > 0 && chunk->chars[offset] == chunk->chars[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->chars[offset]);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
    case OP_CONSTANT:
      return constantInstruction("OP_CONSTANT", chunk, offset);
    case OP_NULL:
      return simpleInstruction("OP_NULL", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
    case OP_POPN:
      return byteInstruction("OP_POPN", chunk, offset);
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return constantInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEFINE_GLOBAL:
      return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
    case OP_SET_GLOBAL:
      return constantInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_MAKE_ARRAY:
      return byteInstruction("OP_MAKE_ARRAY", chunk, offset);
    case OP_GET_INDEX:
      return simpleInstruction("OP_GET_INDEX", offset);
    case OP_SET_INDEX:
      return simpleInstruction("OP_SET_INDEX", offset);
    case OP_GET_SLICE:
      return simpleInstruction("OP_GET_SLICE", offset);
    case OP_SET_SLICE:
      return simpleInstruction("OP_SET_SLICE", offset);
    case OP_GET_UPVALUE:
      return byteInstruction("OP_GET_UPVALUE", chunk, offset);
    case OP_SET_UPVALUE:
      return byteInstruction("OP_SET_UPVALUE", chunk, offset);
    case OP_GET_PROPERTY:
      return constantInstruction("OP_GET_PROPERTY", chunk, offset);
    case OP_SET_PROPERTY:
      return constantInstruction("OP_SET_PROPERTY", chunk, offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
    case OP_EQUAL:
      return simpleInstruction("OP_EQUAL", offset);
    case OP_NEQUAL:
      return simpleInstruction("OP_NEQUAL", offset);
    case OP_GREATER:
      return simpleInstruction("OP_GREATER", offset);
    case OP_GEQUAL:
      return simpleInstruction("OP_GEQUAL", offset);
    case OP_LESS:
      return simpleInstruction("OP_LESS", offset);
    case OP_LEQUAL:
      return simpleInstruction("OP_LEQUAL", offset);
    case OP_INC:
      return simpleInstruction("OP_INC", offset);
    case OP_DEC:
      return simpleInstruction("OP_DEC", offset);
    case OP_DUP:
      return simpleInstruction("OP_DUP", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUBTRACT:
      return simpleInstruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:
      return simpleInstruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:
      return simpleInstruction("OP_DIVIDE", offset);
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_BIN_NOT:
      return simpleInstruction("OP_BIN_NOT", offset);
    case OP_BIN_SHIFTL:
      return simpleInstruction("OP_BIN_SHIFTL", offset);
    case OP_BIN_SHIFTR:
      return simpleInstruction("OP_BIN_SHIFTR", offset);
    case OP_BIN_AND:
      return simpleInstruction("OP_BIN_AND", offset);
    case OP_BIN_OR:
      return simpleInstruction("OP_BIN_OR", offset);
    case OP_BIN_XOR:
      return simpleInstruction("OP_BIN_XOR", offset);
    case OP_PRINT:
      return simpleInstruction("OP_PRINT", offset);
    case OP_JUMP:
      return jumpInstruction("OP_JUMP", 1, chunk, offset);
    case OP_PJMP_IF_FALSE:
      return jumpInstruction("OP_PJMP_IF_FALSE", 1, chunk, offset);
    case OP_QJMP_IF_FALSE:
      return jumpInstruction("OP_QJMP_IF_FALSE", 1, chunk, offset);
    case OP_LOOP:
      return jumpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
    case OP_CLOSURE: {
      offset++;
//      uint8_t constant = chunk->code[offset++];
      uint16_t constant = (chunk->code[offset+0]<<8) | chunk->code[offset+1];
      offset += 2;
      printf("%-16s %04x ", "OP_CLOSURE", constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");

      ObjFunction* function = AS_FUNCTION(
          chunk->constants.values[constant]);
      for (int j = 0; j < function->upvalueCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf("%04x      |                     %s %d\n",
//               offset - 2, isLocal ? "local" : "upvalue", index);
               offset - 3, isLocal ? "local" : "upvalue", index);
      }

      return offset;
    }
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
      return constantInstruction("OP_CLASS", chunk, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}


// Instruction types

//static
int constantInstruction(const char* name, Chunk* chunk, int offset) {
//  uint8_t constant = chunk->code[offset + 1];
  uint16_t constant = (chunk->code[offset + 1]<<8) | chunk->code[offset + 2];
  printf("%-16s %04x '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
//  return offset + 2;
  return offset + 3;
}

/*
static int constantLongInstruction(const char* name, Chunk* chunk, int offset) {
  uint32_t constant = chunk->code[offset + 1];
  constant = constant | (chunk->code[offset + 1] << 8);
  constant = constant | (chunk->code[offset + 1] << 16);
  constant = constant | (chunk->code[offset + 1] << 24);
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}
*/

//static
int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

//static
int byteInstruction(const char* name, Chunk* chunk, int offset) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4x\n", name, slot);
  return offset + 2;
}

//static
int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
//  printf("%-16s %04d -> %04d\n", name, offset, offset + 3 + sign * jump);
  printf("%-16s %04x -> %04x\n", name, offset, offset + 3 + sign * jump);
  return offset + 3;
}

