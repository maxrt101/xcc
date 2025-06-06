# XCC by maxrt101

cmake_minimum_required(VERSION 3.27)

#################################    MACROS    ################################

macro(project_add_inc_recursive)
    foreach (path ${ARGV})
        unset(found_inc)
        file(GLOB_RECURSE found_inc ${path} *.h)
        list(APPEND PROJECT_INCLUDES ${found_inc})
    endforeach ()
endmacro()

macro(project_add_src_recursive)
    foreach (path ${ARGV})
        unset(found_src)
        file(GLOB_RECURSE found_src ${path} *.cc)
        list(APPEND PROJECT_SOURCES ${found_src})
    endforeach ()
endmacro()

################################    PROJECT    ################################

set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-I ${PROJECT_DIR}/include")

project(xcc)

add_compile_options(
    -ggdb -Og
)

project_add_inc_recursive(${PROJECT_DIR}/include)
project_add_src_recursive(${PROJECT_DIR}/src)

add_compile_definitions(
        USE_OPTIMIZATION=0
        USE_CATCH_EXCEPTIONS=0
        USE_PRINT_TOKENS=0
        USE_PRINT_AST=1
        USE_PRINT_LLVM_IR=1
        USE_PRINT_EXPR_RESULT=1
        USE_REPORT_SYMBOL_RESOLVER_SUCCESS=1
        USE_LEGACY_XCC_EXTERN_FUNCTIONS=1
)

add_executable(xcc
  ${PROJECT_INCLUDES}
  ${PROJECT_SOURCES}
)

##################################    LLVM    #################################

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(llvm_libs
        core
        support
        irreader
        orcjit
        x86codegen
        x86asmparser
)

target_link_libraries(xcc ${llvm_libs})
