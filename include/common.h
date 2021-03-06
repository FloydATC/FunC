#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING

typedef void unused; // Silence warnings about unused variables with "(unused) var;"

//#define DEBUG
#ifdef DEBUG
//#define DEBUG_PRINT_CODE
//#define DEBUG_TRACE_SCANNER
//#define DEBUG_TRACE_SCANNER_VERBOSE
//#define DEBUG_TRACE_PARSER
//#define DEBUG_TRACE_PARSER_VERBOSE
//#define DEBUG_TRACE_COMPILER
//#define DEBUG_TRACE_CHUNK
//#define DEBUG_TRACE_VALUES
//#define DEBUG_TRACE_OBJECTS
//#define DEBUG_TRACE_TABLES
//#define DEBUG_TRACE_EXECUTION
//#define DEBUG_TRACE_MEMORY
//#define DEBUG_TRACE_MEMORY_VERBOSE
//#define DEBUG_TRACE_MEMORY_HEXDUMP
//#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC
//#define DEBUG_LOG_GC_VERBOSE
//#define DEBUG_LOG_GC_HEXDUMP
//#define DEBUG_LOG_GC_EXTREME
#endif // DEBUG

#define UINT8_COUNT (UINT8_MAX + 1)
#define MAX_BREAKS_PER_SCOPE 256
//#define MAX_SWITCH_CASES 256



// For debugging, optionally disable OP_INVOKE
#define OPTIMIZE_METHOD_CALLS



#endif // clox_common_h

