include(ExternalProject)

message(STATUS "LLD: Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "LLD: Using llvm-config in: ${LLVM_CONFIG}")
set(LLVM_DIR "${CMAKE_BINARY_DIR}/deps")
set(LLVM_CONFIG "${LLVM_DIR}/bin/llvm-config")


set(prefix ${CMAKE_BINARY_DIR}/deps)
set(source_dir ${prefix}/src/lld)
set(binary_dir ${prefix}/src/lld-build)
set(include_dir ${source_dir}/include)

set(runtime_library ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldCommon${CMAKE_STATIC_LIBRARY_SUFFIX})
set(other_libraries 
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldCore${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldDriver${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldReaderWriter${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldWasm${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldCommon${CMAKE_STATIC_LIBRARY_SUFFIX} 

    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldCore${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldDriver${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldReaderWriter${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldWasm${CMAKE_STATIC_LIBRARY_SUFFIX}
    ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldCommon${CMAKE_STATIC_LIBRARY_SUFFIX} 
)
# set(other_libraries ${binary_dir}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lldWasm${CMAKE_STATIC_LIBRARY_SUFFIX})
# message(STATUS ${other_libraries})

set(cxx_flags "-std=c++14 -Wno-error")
set(c_flags "-Wno-error")


ExternalProject_Add(lld
    PREFIX ${prefix}
    DOWNLOAD_DIR ${prefix}/downloads
    SOURCE_DIR ${source_dir}
    BINARY_DIR ${binary_dir}
    URL https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/lld-10.0.0.src.tar.xz
    URL_HASH SHA256=b9a0d7c576eeef05bc06d6e954938a01c5396cee1d1e985891e0b1cf16e3d708
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    -DCMAKE_BUILD_TYPE=Release
    -DLLVM_CONFIG_PATH=${LLVM_CONFIG}
    -DLLVM_INCLUDE_TESTS=OFF
    -DLLVM_INSTALL_TOOLCHAIN_ONLY=OFF
    -DCMAKE_CXX_FLAGS=${cxx_flags}
    -DCMAKE_C_FLAGS=${c_flags}
    INSTALL_COMMAND ""
    BUILD_BYPRODUCTS ${other_libraries}
)

# message(STATUS "LLD: BEFORE EXT2")

file(MAKE_DIRECTORY ${include_dir})  # Must exist.

add_library(lld::lld STATIC IMPORTED)
set_target_properties(
    lld::lld
    PROPERTIES
    IMPORTED_CONFIGURATIONS Release
    IMPORTED_LOCATION_RELEASE ${runtime_library}
    INTERFACE_INCLUDE_DIRECTORIES "${include_dir}"
    INTERFACE_LINK_LIBRARIES "${other_libraries}"
)

add_dependencies(lld::lld lld)
