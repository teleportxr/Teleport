enable_testing()

if(TELEPORT_TEST)

function(SETUP_TESTS)
    foreach(_testname ${ARGN})
        add_executable(${_testname} test_${_testname}.cpp )
        add_test(NAME ${_testname} COMMAND ${_testname})
        target_link_libraries(${_testname} PRIVATE Catch2::Catch2WithMain)
    endforeach()
endfunction()

SETUP_TESTS(
   anim
)
endif()