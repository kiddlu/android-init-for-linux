cmake_minimum_required(VERSION 2.8)
project (init)

add_definitions (
   -pipe -O2 -march=native
   -Wall -Wno-unused-parameter
   -DBOOTCHART=1
)

set(INIT_SOURCES
 ${PROJECT_SOURCE_DIR}/init/builtins.c
 
 ${PROJECT_SOURCE_DIR}/init/init.c
 
 ${PROJECT_SOURCE_DIR}/init/parser.c
 ${PROJECT_SOURCE_DIR}/init/util.c
 ${PROJECT_SOURCE_DIR}/init/devices.c
 
 ${PROJECT_SOURCE_DIR}/init/strlcpy.c
 ${PROJECT_SOURCE_DIR}/init/bootchart.c
)

#static link
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

include_directories (${PROJECT_SOURCE_DIR}/init/)

add_executable(init ${INIT_SOURCES})
target_link_libraries(init)