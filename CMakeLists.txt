cmake_minimum_required(VERSION 3.25)
project(
  pxkorka
  VERSION 0.1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- OPTIONS ---
option(ENABLE_TESTS "Build tests" ON)

# Enable all warnings
if(MSVC)
  add_compile_options(/W4 /WX /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wshadow -Wunused)
endif()

include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/3party)

FetchContent_Declare(
  frozen
  GIT_REPOSITORY "https://github.com/serge-sans-paille/frozen.git"
  GIT_TAG master
  GIT_SHALLOW ON)

FetchContent_Declare(
  catch2
  GIT_REPOSITORY "https://github.com/catchorg/Catch2.git"
  GIT_TAG v3.13.0
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(frozen catch2)

# # Dependencies include(cmake/CPM.cmake)

# if(ENABLE_TESTS) cpmaddpackage("gh:catchorg/Catch2@3.13.0") list(APPEND
# CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras) include(Catch) endif()

file(GLOB_RECURSE KORKA_LIB_HEADERS include/korka/*.hpp)

message(NOTICE ${KORKA_LIB_HEADERS})

add_library(korka_lib src/vm/vm_runtime.cpp ${KORKA_LIB_HEADERS})

target_include_directories(
  korka_lib
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:include>
  INTERFACE src)

add_executable(pxkorka main.cpp)
target_link_libraries(pxkorka PRIVATE korka_lib frozen::frozen Catch2)

# Ignore unused-but-set-variable shadow
target_compile_options(korka_lib PUBLIC -Wno-error=unused-but-set-variable
                                        -Wno-error=shadow)

# --- TESTS ---
# if (ENABLE_TESTS) enable_testing()
#
# add_executable(pxkorka_tests test/lexer.cpp test/bytecode_builder.cpp
# test/parser.cpp )
#
# target_link_libraries(pxkorka_tests PRIVATE korka_lib Catch2WithMain )
#
# catch_discover_tests(pxkorka_tests) endif ()
