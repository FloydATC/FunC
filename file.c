#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "file.h"
#include "vm.h"
#include "memory.h"

int readFile(const char* path, char** buffer) {
  printf("file:readFile() path=%s\n", path);
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
      printf("Current working dir: %s\n", cwd);
    }
    return(-74);
  }
  printf("file:readFile() open successful, seek to end\n");

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  printf("file:readFile() file size is %d bytes, rewinding\n", (int)fileSize);
  rewind(file);

  printf("file:readFile() allocating buffer\n");
  *buffer = (char*)malloc(fileSize + 1);
  if (*buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    return(-74);
  }
  printf("file:readFile() reading file\n");
  size_t bytesRead = fread(*buffer, sizeof(char), fileSize, file);
  printf("file:readFile() read %d bytes\n", (int) bytesRead);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    free(*buffer);
    *buffer = NULL;
    return(-74);
  }
  printf("file:readFile() terminating buffer at %p\n", (*buffer+bytesRead));
  (*buffer)[bytesRead] = '\0';

  fclose(file);
  printf("file:readFile() file closed\n");
  return bytesRead;
}


// Experimental include support

int addFilename(void* vm, const char* filename) {
  // If file already exists, return existing index
  int fileno = getFilenoByName(vm, filename);
  if (fileno != -1) return fileno;

  // Internalize filename
  Value fname = OBJ_VAL(copyString(vm, filename, strlen(filename)));

  // Add the value to array
  push(vm, fname);
  writeValueArray(vm, &((VM*)vm)->filenames, fname);
  pop(vm);

  // Return the index
  return ((VM*)vm)->filenames.count - 1;
}

// This function is typically used to check of the file has already been included
// Return -1 if the filename is not found in vm->filenames
// Otherwise return the index
int getFilenoByName(void* vm, const char* filename) {
  // Internalize filename
  Value fname = OBJ_VAL(copyString(vm, filename, strlen(filename)));

  // Scan ValueArray and check for equality
  for (int i=0; i<((VM*)vm)->filenames.count; i++) {
    if (valuesEqual( ((VM*)vm)->filenames.values[i], fname )) return i;
  }
  return -1; // Not found
}

// Given a fileno, return the filename as a Value
Value getFilenameByNo(void* vm, int id) {
  if (id < 0) return OBJ_VAL(copyString(vm, "script", 6)); // -1 = top level script
  if (id > ((VM*)vm)->filenames.count - 1) return OBJ_VAL(copyString(vm, "unknown", 7)); // invalid id
  return ((VM*)vm)->filenames.values[id];
}


