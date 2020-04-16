#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  ObjString* key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacityMask;
  Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(void* vm, Table* table);
bool tableGet(void* vm, Table* table, ObjString* key, Value* value);
bool tableSet(void* vm, Table* table, ObjString* key, Value value);
bool tableDelete(void* vm, Table* table, ObjString* key);
void tableAddAll(void* vm, Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(void* vm, Table* table);
void markTable(void* vm, Table* table);

#endif

