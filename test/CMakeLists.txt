list(APPEND tests
  kill
  spawn
  spawn-bad-file
  spawn-exit-status
)

foreach(test IN LISTS tests)
  add_executable(${test} ${test}.c)

  target_link_libraries(${test} tt_shared)

  add_test(
    NAME ${test}
    COMMAND ${test}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach()
