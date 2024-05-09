cmake_minimum_required(VERSION 3.22)

project(tt C)

if(NOT TARGET uv)
  add_subdirectory(vendor/libuv EXCLUDE_FROM_ALL)
endif()

add_library(tt OBJECT)

set_target_properties(
  tt
  PROPERTIES
  C_STANDARD 99
  POSITION_INDEPENDENT_CODE ON
)

target_sources(
  tt
  INTERFACE
    include/tt.h
  PRIVATE
    src/pty.c
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
    $<TARGET_PROPERTY:uv,INTERFACE_INCLUDE_DIRECTORIES>
)

add_library(tt_shared SHARED)

set_target_properties(
  tt_shared
  PROPERTIES
  OUTPUT_NAME tt
  WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(
  tt_shared
  PUBLIC
    tt
    uv
)

if(UNIX)
  target_link_libraries(
    tt_shared
    PUBLIC
      -lutil
  )
endif()

add_library(tt_static STATIC)

set_target_properties(
  tt_static
  PROPERTIES
  OUTPUT_NAME tt
)

target_link_libraries(
  tt_static
  PUBLIC
    tt
    uv_a
)

if(UNIX)
  target_link_libraries(
    tt_static
    PUBLIC
      -lutil
  )
endif()

install(TARGETS tt_shared tt_static)

install(FILES include/tt.h DESTINATION include)

install(DIRECTORY include/tt DESTINATION include)

if(PROJECT_IS_TOP_LEVEL)
  enable_testing()

  add_subdirectory(test)
endif()
