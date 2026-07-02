cmake_minimum_required(VERSION 3.4.0)

cmake_policy(SET CMP0042 OLD)	# Fix MACOSX_RPATH.
cmake_policy(SET CMP0048 NEW)	# Allow VERSION argument in project().
if (POLICY CMP0054)
	cmake_policy(SET CMP0054 NEW)	# No longer implicitly dereference variables.
endif()

set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo)

# project(EVMTRANS VERSION 0.9.0.2 LANGUAGES CXX C)
# project(EVMTRANS VERSION 0.1.0 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

message(STATUS "EVMTRANS ${EVMTRANS_VERSION_MAJOR}.${EVMTRANS_VERSION_MINOR}.${EVMTRANS_VERSION_PATCH}")

if (NOT ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64|AMD64")
	message(FATAL_ERROR "Target ${CMAKE_SYSTEM_PROCESSOR} not supported -- EVM JIT works only on x86_64 architecture")
endif()

option(EVMTRANS_EXAMPLES "Generate build targets for the EVMTRANS examples" OFF)
option(EVMTRANS_TESTS "Create targets for CTest" OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Always use Release variant of C++ runtime.
	# We don't want to provide Debug variants of all dependencies. Some default
	# flags set by CMake must be tweaked.
	string(REPLACE "/MDd" "/MD" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	string(REPLACE "/D_DEBUG" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
	set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS OFF)
else()
	set(CMAKE_CXX_FLAGS "-std=c++17 -Wall -Wextra -Wconversion -Wno-sign-conversion -Wno-unknown-pragmas ${CMAKE_CXX_FLAGS}")
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT SANITIZE)
	# Do not allow unresolved symbols in shared library (default on linux)
	# unless sanitizer is used (sanity checks produce unresolved function calls)
	set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
endif()


add_subdirectory(evmc)

project(EVMTRANS VERSION 0.1.0 LANGUAGES CXX C)


include(ProjectLLVM)
configure_llvm_project()

include(ProjectLLD)

option(HERA_BINARYEN "Build with binaryen" OFF)
# if (HERA_BINARYEN)
include(ProjectBinaryen)
# endif()

option(HERA_WABT "Build with wabt" ON)
if (HERA_WABT)
	include(ProjectWabt)
endif()

option(HERA_WAVM "Build with WAVM" OFF)
if (HERA_WAVM)
    include(ProjectWAVM)
endif()

if (NOT (HERA_BINARYEN OR HERA_WABT OR HERA_WAVM))
    message(FATAL_ERROR "At least one one engine must be enabled.")
endif()

add_subdirectory(evmtrans)


# if (EVMTRANS_TESTS)
# add_subdirectory(tests)
# enable_testing()
# endif()