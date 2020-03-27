#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
//#include <Sysinfoapi.h>
#include <windows.h>
#include <direct.h>
//#include <dirent_msvc.h>
#define PATH_MAX 260
#else
#include <unistd.h>
#endif

#include "file.h"
#include "vm.h"
#include "memory.h"



int readFile(const char* path, char** buffer) {
#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() path='%s'\n", path);
#endif
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file '%s'.\n", path);
    char cwd[PATH_MAX];
    if (_getcwd(cwd, sizeof(cwd)) != (char*)NULL) {
      printf("Current working dir: %s\n", cwd);
    }
    return(-74);
  }
#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() open successful, seek to end\n");
#endif

  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() file size is %d bytes, rewinding\n", (int)fileSize);
#endif
  rewind(file);

#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() allocating buffer\n");
#endif
  *buffer = (char*)malloc(fileSize + 1);
  if (*buffer == NULL) {
    fprintf(stderr, "Not enough memory to read '%s'.\n", path);
    return(-74);
  }
#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() reading file\n");
#endif
  size_t bytesRead = fread(*buffer, sizeof(char), fileSize, file);
#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() read %d bytes\n", (int) bytesRead);
#endif
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file '%s'.\n", path);
    free(*buffer);
    *buffer = NULL;
    return(-74);
  }
#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() terminating buffer at %p\n", (*buffer+bytesRead));
#endif
  (*buffer)[bytesRead] = '\0';

  fclose(file);
#ifdef DEBUG_TRACE_FILE
  printf("file:readFile() file closed\n");
#endif
  return (int)bytesRead;
}


// Experimental include support

int addFilename(void* vm, const char* filename) {
  // If file already exists, return existing index
  int fileno = getFilenoByName(vm, filename);
  if (fileno != -1) return fileno;

  // Internalize filename
  Value fname = OBJ_VAL(copyString(vm, filename, (int)strlen(filename)));

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
  Value fname = OBJ_VAL(copyString(vm, filename, (int)strlen(filename)));

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


