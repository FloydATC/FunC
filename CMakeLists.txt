
cmake_minimum_required(VERSION 3.10)

project(func VERSION 1.2.3.4)
set(func_VERSION_STATUS "EXPERIMENTAL")

set(INCLUDE_DIR "include/")
set(SOURCE_DIR "src/")

set(SOURCES
	src/chunk.c
	src/compiler.c
	src/debug.c
	src/error.c
	src/file.c
	src/index.c
	src/memory.c
	src/number.c
	src/objarray.c
	src/object.c
	src/objnumber.c
	src/objstring.c
	src/parser.c
	src/scanner.c
	src/table.c
	src/utf8.c
	src/value.c
	src/vm.c
)

add_library(FunCx64 SHARED ${SOURCES})
add_executable(func src/main.c)

include_directories(
	"${INCLUDE_DIR}"
)

# <math.h>
target_link_libraries(FunCx64 m)
target_link_libraries(func m)
target_link_libraries(func FunCx64)

include(GNUInstallDirs)
install(TARGETS FunCx64 
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(TARGETS func
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
