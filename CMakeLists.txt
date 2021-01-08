cmake_minimum_required(VERSION 2.8)
project (init)

add_definitions (
   -pipe -O2
   -Wno-unused-parameter
   -Wno-unused-variable
   -Wno-unused-result
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

 ${PROJECT_SOURCE_DIR}/init/propd.c
)

#static link
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

add_executable(init ${INIT_SOURCES})
target_link_libraries(init)

add_library(prop STATIC ${PROJECT_SOURCE_DIR}/libprop/properties.c)

add_executable(service  ${PROJECT_SOURCE_DIR}/libprop/service.c)
target_link_libraries(service prop pthread)

add_executable(getprop  ${PROJECT_SOURCE_DIR}/libprop/getprop.c)
target_link_libraries(getprop prop pthread)

add_executable(setprop  ${PROJECT_SOURCE_DIR}/libprop/setprop.c)
target_link_libraries(setprop prop pthread)

