# XCC by maxrt101

cmake_minimum_required(VERSION 3.27)

#################################    COLORS    ################################

string(ASCII 27 ESC)
set(COLOR_RESET     "${ESC}[m")
set(COLOR_RED       "${ESC}[31m")
set(COLOR_GREEN     "${ESC}[32m")
set(COLOR_YELLOW    "${ESC}[33m")
set(COLOR_BLUE      "${ESC}[34m")
set(COLOR_MAGENTA   "${ESC}[35m")
set(COLOR_CYAN      "${ESC}[36m")
set(COLOR_WHITE     "${Esc}[37m")

#################################    MACROS    ################################

macro(add_inc_recursive)
    foreach (path ${ARGV})
        unset(found_inc)
        file(GLOB_RECURSE found_inc ${path} *.h)
        list(APPEND PROJECT_INCLUDES ${found_inc})
    endforeach ()
endmacro()

macro(add_src_recursive)
    foreach (path ${ARGV})
        unset(found_src)
        file(GLOB_RECURSE found_src ${path} *.cc)
        list(APPEND PROJECT_SOURCES ${found_src})
    endforeach ()
endmacro()

macro(feature_parse_name_value feature name value)
    string(REPLACE "=" ";" feature_tokens ${feature})
    list(GET feature_tokens 0 ${name})
    list(GET feature_tokens 1 ${value})
endmacro()

macro(feature_get_value feature value)
    if (DEFINED ${feature})
        set(${value} ${${feature}})
    elseif (DEFINED ENV{${feature}})
        set(${value} $ENV{${feature}})
    else()
        set(${value} UNDEFINED)
    endif ()
endmacro()

################################    PROJECT    ################################

set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(CMAKE_CXX_STANDARD 20)

project(xcc)

include_directories(${PROJECT_DIR}/include)

add_inc_recursive(${PROJECT_DIR}/include)
add_src_recursive(${PROJECT_DIR}/src)

add_compile_options(
    -ggdb -Og
)

add_link_options(
    # Has no effect on macos, but without this on linux
    # xcc won't resolve symbols from running process as
    # linux doesn't export host binary symbols into the
    # process
    -rdynamic
)

add_executable(xcc
  ${PROJECT_INCLUDES}
  ${PROJECT_SOURCES}
)

################################    FEATURES    ################################

set(FEATURE_TOGGLES
        USE_OPTIMIZATION=0
        USE_CATCH_EXCEPTIONS=1
        USE_PRINT_TOKENS=0
        USE_PRINT_AST=0
        USE_PRINT_LLVM_IR=0
        USE_PRINT_EXPR_RESULT=1
        USE_PRINT_LLVM_IR_ON_VERIFY_FAIL=1
        USE_DUMP_JIT=0
        USE_REPORT_SYMBOL_RESOLVER_SUCCESS=0
        USE_LEGACY_XCC_EXTERN_FUNCTIONS=1
)

foreach (feature ${FEATURE_TOGGLES})
    feature_parse_name_value(${feature} name default)
    feature_get_value(${name} val)

    if (${val} STREQUAL UNDEFINED)
        message(STATUS "${COLOR_BLUE}Feature ${name} using default value ${default}${COLOR_RESET}")
        add_compile_definitions(${feature})
    else ()
        message(STATUS "${COLOR_YELLOW}Feature ${name} overridden, value ${val}${COLOR_RESET}")
        add_compile_definitions(${name}=${val})
    endif ()
endforeach ()

##################################    VERBOSE    #################################

feature_get_value(VERBOSE verbose)
if ("${verbose}" STREQUAL "1")
    message(STATUS "${COLOR_MAGENTA}Verbose makefiles ON${COLOR_RESET}")
    set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "Verbose makefile" FORCE)
endif ()

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

##################################    STATUS    #################################

message(STATUS "${COLOR_CYAN}Compiler:     ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (${CMAKE_CXX_COMPILER})${COLOR_RESET}")
message(STATUS "${COLOR_CYAN}Linker:       ${CMAKE_CXX_COMPILER_LINKER_ID} ${CMAKE_CXX_COMPILER_LINKER_VERSION} (${CMAKE_CXX_COMPILER_LINKER})${COLOR_RESET}")
message(STATUS "${COLOR_CYAN}LLVM version: ${LLVM_PACKAGE_VERSION} ($ENV{LLVM_DIR})${COLOR_RESET}")
