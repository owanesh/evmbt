project(tests)

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}/include")
set(PROJECT_TEST_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/tests/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_TEST_BINARY_DIR})
set(CMAKE_CXX_STANDARD 11)


add_executable(test-se test-se.cpp)
# get_filename_component(EVMTRANS_INCLUDE_DIR ../include ABSOLUTE)
# target_include_directories(test-se PRIVATE ../include)


# message(STATUS "$EVMTRANS_STANDALONE_LIB: ${EVMTRANS_STANDALONE_LIB}")

# target_include_directories(test-se  SYSTEM PRIVATE ${LLVM_INCLUDE_DIRECTORIES})
# target_include_directories(test-se PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen)
# target_include_directories(test-se PUBLIC ${EVMTRANS_INCLUDE_DIR})

# target_link_libraries(test-se PRIVATE evmc lld::lld LLVM::JIT)
# get_target_property(LLVM_INCLUDE_DIRECTORIES LLVM::JIT INTERFACE_INCLUDE_DIRECTORIES)
# target_include_directories(test-se SYSTEM PRIVATE ${LLVM_INCLUDE_DIRECTORIES})
# message(STATUS "EVMTRANS_INCLUDE_DIR = ${EVMTRANS_INCLUDE_DIR}")

# target_link_libraries(test-se  ${EVMTRANS_STANDALONE_LIB} evmc )
# target_link_libraries(test-se  ${LLVM_INCLUDE_DIRECTORIES})
# target_link_libraries(test-se  ${EVMTRANS_INCLUDE_DIR})
find_package(Z3 REQUIRED)
target_link_libraries(test-se  ${Z3_LIB})

target_include_directories(test-se PRIVATE ${Z3_CXX_INCLUDE_DIRS})
target_link_libraries(test-se PRIVATE ${Z3_LIBRARIES})


# Compile standalone evmtrans
# add_executable(standalone-evmtrans standalone-evmtrans.cpp ${SOURCES_LIBS})
# add_dependencies(standalone-evmtrans LLVM::JIT)
# if (LLVM_COMPILE_DEFINITIONS)
# 	target_compile_definitions(test-se  PRIVATE ${LLVM_COMPILE_DEFINITIONS})
# endif()
# target_include_directories(test-se  SYSTEM PRIVATE ${LLVM_INCLUDE_DIRECTORIES})

# target_link_libraries(test-se PRIVATE lld::lld LLVM::JIT)


enable_testing()

# add_test(usage_test test-se)
# set_tests_properties(usage_test PROPERTIES WILL_FAIL TRUE)

# add_test(fail_test test-se)
# set_tests_properties(fail_test PROPERTIES PASS_REGULAR_EXPRESSION "FAIL")

# # Library tests:


