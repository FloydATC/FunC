#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

// Double the bucket count when hash is 75% "full", disregarding the
// possibility that some buckets may already have more than one entry in it
#define TABLE_MAX_LOAD 0.75



void initTable(Table* table) {
  table->count = 0;
  table->capacityMask = -1;
  table->entries = NULL;
}

void freeTable(void* vm, Table* table) {
  FREE_ARRAY(vm, Entry, table->entries, table->capacityMask + 1);
  initTable(table);
}

static Entry* findEntry(Entry* entries, int capacityMask,
                        ObjString* key) {
  uint32_t index = key->hash & capacityMask;
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];

    if (entry->key == NULL) {
      if (IS_NULL(entry->value)) {
        // Empty entry.
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone.
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (entry->key == key) {
      // We found the key.
      // We can use == because internalized strings guarantee
      // only one Obj pointer can refer to each unique string
      return entry;
    }

    index = (index + 1) & capacityMask;
  }
}

// If 'key' exists in table, set 'value' and return true
// Otherwise, return false
bool tableGet(void* vm, Table* table, ObjString* key, Value* value) {
  (unused)vm;
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacityMask, key);
  if (entry->key == NULL) return false;

  *value = entry->value;
  return true;
}

static void adjustCapacity(void* vm, Table* table, int capacityMask) {
#ifdef DEBUG_TRACE_TABLES
  printf("table:adjustCapacity() table=%p capacityMask=%d\n", table, capacityMask);
#endif
  Entry* entries = ALLOCATE(vm, Entry, capacityMask + 1);
  // Initialize new table
  for (int i = 0; i <= capacityMask; i++) {
    entries[i].key = NULL;
    entries[i].value = NULL_VAL;
  }

  // Capacity changed -> modulo changed, and so
  // all existing entries must be re-inserted into the new empty array.
  // This may cause previously "probed" entries to find a place
  // in "their own" bucket and /improve/ lookup speed as the hash grows.
  // Table grows by a factor of 2 so this reorg happens less frequently with size
  // but crossing a n**2 boundary does trigger the following loop:
  table->count = 0;
  for (int i = 0; i <= table->capacityMask; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue;

    Entry* dest = findEntry(entries, capacityMask, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  // Dispose of the old array:
  // FREE_ARRAY(type, oldPointer, oldCount) in memory.h
  FREE_ARRAY(vm, Entry, table->entries, table->capacityMask + 1);

  // Replace with new pointer and capacity
  table->entries = entries;
  table->capacityMask = capacityMask;
}

// Insert (key = value) into table
bool tableSet(void* vm, Table* table, ObjString* key, Value value) {
  // Make sure the table exists and is big enough
  if (table->count + 1 > (table->capacityMask + 1) * TABLE_MAX_LOAD) {
    int capacityMask = GROW_CAPACITY(table->capacityMask + 1) - 1;
    adjustCapacity(vm, table, capacityMask);
  }

  // Does this key already exist?
  Entry* entry = findEntry(table->entries, table->capacityMask, key);

  bool isNewKey = entry->key == NULL;
  if (isNewKey && IS_NULL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

// If we 'probed' a key into a neighbor bucket, there may be
// other keys past this key so we can't simply delete or those
// keys would be cut off. Instead, leave a 'tombstone' key.
bool tableDelete(void* vm, Table* table, ObjString* key) {
  (unused)vm;
  if (table->count == 0) return false;

  // Find the entry.
  Entry* entry = findEntry(table->entries, table->capacityMask, key);
  if (entry->key == NULL) return false;

  // Place a tombstone in the entry.
  entry->key = NULL;
  entry->value = BOOL_VAL(true);

  return true;
}

// Copy entries in table 'from' into table 'to'
void tableAddAll(void* vm, Table* from, Table* to) {
  for (int i = 0; i <= from->capacityMask; i++) {
    Entry* entry = &from->entries[i];
    if (entry->key != NULL) {
      tableSet(vm, to, entry->key, entry->value);
    }
  }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash & table->capacityMask;

  for (;;) {
    Entry* entry = &table->entries[index];

    if (entry->key == NULL) {
      // Stop if we find an empty non-tombstone entry.
      if (IS_NULL(entry->value)) return NULL;
    } else if (entry->key->length == length &&
        entry->key->hash == hash &&
        memcmp(entry->key->chars, chars, length) == 0) {
      // We found it.
      return entry->key;
    }

    index = (index + 1) & table->capacityMask;
  }
}

void tableRemoveWhite(void* vm, Table* table) {
  for (int i = 0; i <= table->capacityMask; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->obj.isMarked) {
      tableDelete(vm, table, entry->key);
    }
  }
}

void markTable(void* vm, Table* table) {
  for (int i = 0; i <= table->capacityMask; i++) {
    Entry* entry = &table->entries[i];
    markObject(vm, (Obj*)entry->key);
    markValue(vm, entry->value);
  }
}

