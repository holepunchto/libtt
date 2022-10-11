cmake_minimum_required(VERSION 3.22)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

project(tt C)

add_subdirectory(vendor/libuv EXCLUDE_FROM_ALL)

add_library(tt OBJECT)

set_target_properties(
  tt
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE 1
)

target_sources(
  tt
  INTERFACE
    include/tt.h
)

if(UNIX)
  target_sources(
    tt
    PRIVATE
      src/unix/pty.c
  )
endif()

if(WIN32)
  target_sources(
    tt
    PRIVATE
      src/win/pty.c
  )
endif()

target_include_directories(
  tt
  PUBLIC
    include
    $<TARGET_PROPERTY:uv,INCLUDE_DIRECTORIES>
)

add_library(tt_shared SHARED $<TARGET_OBJECTS:tt>)

set_target_properties(
  tt_shared
  PROPERTIES
  OUTPUT_NAME tt
)

target_link_libraries(
  tt_shared
  PUBLIC
    uv
)

add_library(tt_static STATIC $<TARGET_OBJECTS:tt>)

set_target_properties(
  tt_static
  PROPERTIES
  OUTPUT_NAME tt
)

target_link_libraries(
  tt_static
  PUBLIC
    uv
)

install(TARGETS tt_shared tt_static)

install(FILES include/tt.h DESTINATION include)
install(DIRECTORY include/tt DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(test)
endif()
