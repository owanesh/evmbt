get_filename_component(EVMTRANS_INCLUDE_DIR ../include ABSOLUTE)

set(hera ${PROJECT_SOURCE_DIR}/evmtrans/hera)

include_directories(${hera})

set(SOURCES_ENGINES
	BinaryTrans.cpp		BinaryTrans.h
	${hera}/hera.h		${hera}/hera.cpp
	${hera}/debugging.h	${hera}/exceptions.h
	${hera}/eei.cpp		${hera}/eei.h
	${hera}/helpers.cpp	${hera}/helpers.h
)

# set(SOURCES_LIBS
# 	BasicBlock.cpp		BasicBlock.h
# 	Arith256.cpp		Arith256.h
# 	SECore.cpp			SECore.h
# 	      Tainter.h
# 						Common.h
# 	CompilerHelper.cpp	CompilerHelper.h
# 	Instruction.cpp		Instruction.h
# 	Type.cpp			Type.h
# 	Optimizer.cpp		Optimizer.h
# 	CodeGen.cpp 		CodeGen.h
# 	EEIModule.cpp		EEIModule.h
# 	WASMCompiler.cpp	WASMCompiler.h
# )

set(SOURCES_LIBS
	BasicBlock.cpp		BasicBlock.h
	Arith256.cpp		Arith256.h
						Common.h
	CompilerHelper.cpp	CompilerHelper.h
	Instruction.cpp		Instruction.h
	Type.cpp			Type.h
	Optimizer.cpp		Optimizer.h
	CodeGen.cpp 		CodeGen.h
	EEIModule.cpp		EEIModule.h
	NEARModule.cpp      NEARModule.h
	WASMCompiler.cpp	WASMCompiler.h
)

set(SOURCES ${SOURCES_LIBS} ${SOURCES_ENGINES})




source_group("" FILES ${SOURCES})

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -fno-rtti -fvisibility=hidden")
	if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,ALL") # Do not export symbols from dependies, mostly LLVM libs
	endif()
endif()

if(EVMTRANS_VERSION_MAJOR EQUAL 0)
	set(EVMTRANS_SOVERSION "0.${EVMTRANS_VERSION_MINOR}")
else()
	set(EVMTRANS_SOVERSION ${EVMTRANS_VERSION_MAJOR})
endif()


string(COMPARE EQUAL "${LLVM_ENABLE_ASSERTIONS}" "ON" LLVM_DEBUG)
configure_file(BuildInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/gen/BuildInfo.gen.h)

add_library(evmtrans ${SOURCES} gen/BuildInfo.gen.h)

if(HERA_BINARYEN)
  target_sources(evmtrans PRIVATE ${hera}/binaryen.cpp ${hera}/binaryen.h)
endif()

if(HERA_WABT)
  target_sources(evmtrans PRIVATE ${hera}/wabt.cpp ${hera}/wabt.h)
endif()

if(HERA_WAVM)
  target_sources(evmtrans PRIVATE ${hera}/wavm.cpp ${hera}/wavm.h)
endif()

# Explicit dependency on llvm to download LLVM header files.
add_dependencies(evmtrans LLVM::JIT)
get_target_property(LLVM_COMPILE_DEFINITIONS LLVM::JIT INTERFACE_COMPILE_DEFINITIONS)
if (LLVM_COMPILE_DEFINITIONS)
	target_compile_definitions(evmtrans PRIVATE ${LLVM_COMPILE_DEFINITIONS})
endif()
get_target_property(LLVM_INCLUDE_DIRECTORIES LLVM::JIT INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(evmtrans SYSTEM PRIVATE ${LLVM_INCLUDE_DIRECTORIES})
target_include_directories(evmtrans PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen)
target_include_directories(evmtrans PUBLIC ${EVMTRANS_INCLUDE_DIR})

target_link_libraries(evmtrans PRIVATE evmc lld::lld LLVM::JIT)

if(HERA_BINARYEN)
	target_compile_definitions(evmtrans PRIVATE HERA_BINARYEN=1)
endif()
target_link_libraries(evmtrans PRIVATE binaryen::binaryen)

if(HERA_WABT)
    target_compile_definitions(evmtrans PRIVATE HERA_WABT=1)
    target_link_libraries(evmtrans PRIVATE wabt::wabt)
endif()

if(HERA_WAVM)
    target_compile_definitions(evmtrans PRIVATE HERA_WAVM=1)
    target_link_libraries(evmtrans PRIVATE wavm::wavm)
endif()

set_target_properties(evmtrans PROPERTIES
	VERSION ${EVMTRANS_VERSION}
	SOVERSION ${EVMTRANS_SOVERSION}
	FOLDER "libs"
)
# target_compile_features(evmtrans PRIVATE cxx_std_17)

include(GNUInstallDirs)
install(TARGETS evmtrans
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY ${EVMTRANS_INCLUDE_DIR}/
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# This section configures evmtrans-standalone -- a static library containing
# evmtrans and all its static dependencies.

# This function tries to get a list of all static library dependencies of
# a target. It probably does not handle all cases to be generic, but works for
# LLVM case.
function(get_link_libraries OUTPUT_LIST TARGET)
	get_target_property(IMPORTED ${TARGET} IMPORTED)
	list(APPEND VISITED_TARGETS ${TARGET})
	if (IMPORTED)
		get_target_property(LIBS ${TARGET} INTERFACE_LINK_LIBRARIES)
	else()
		get_target_property(LIBS ${TARGET} LINK_LIBRARIES)
	endif()
	set(LIB_FILES "")
	foreach(LIB ${LIBS})
		if (TARGET ${LIB})
            get_target_property(type ${LIB} TYPE)
            if(type STREQUAL INTERFACE_LIBRARY)
                continue()  # Skip interface libraries.
            endif()
			list(FIND VISITED_TARGETS ${LIB} VISITED)
			if (${VISITED} EQUAL -1)
				get_target_property(LIB_FILE ${LIB} LOCATION)
				get_link_libraries(LINK_LIB_FILES ${LIB})
				list(APPEND LIB_FILES ${LIB_FILE} ${LINK_LIB_FILES})
			endif()
		elseif (IS_ABSOLUTE ${LIB})
			# Here LIB is a full path to a (hopefuly static) library. We don't
			# want system libs (usually dynamic) here.
			list(APPEND LIB_FILES ${LIB})
		endif()
	endforeach()
	set(VISITED_TARGETS ${VISITED_TARGETS} PARENT_SCOPE)
	list(REMOVE_DUPLICATES LIB_FILES)
	set(${OUTPUT_LIST} ${LIB_FILES} PARENT_SCOPE)
endfunction()

# When building static lib add additional target evmtrans-standalone --
# an archive containing all LLVM dependencies in a single file.
get_target_property(_evmtrans_type evmtrans TYPE)
if (_evmtrans_type STREQUAL STATIC_LIBRARY)
	get_link_libraries(EVMTRANS_LINK_LIBRARIES evmtrans)
	set(EVMTRANS_STANDALONE_FILE ${CMAKE_STATIC_LIBRARY_PREFIX}evmtrans-standalone${CMAKE_STATIC_LIBRARY_SUFFIX})
	if (MSVC)
		set(_dummy_file ${CMAKE_CURRENT_BINARY_DIR}/evmtrans-standalone.c)
		if (NOT EXISTS ${_dummy_file})
			file(WRITE ${_dummy_file} "// evmtrans-standalone dummy file")
		endif()
		add_library(evmtrans-standalone STATIC EXCLUDE_FROM_ALL ${_dummy_file})
		add_dependencies(evmtrans-standalone evmtrans)
		string(REPLACE ";" " " FLAGS "${EVMTRANS_LINK_LIBRARIES}")
		set(FLAGS "${CMAKE_CFG_INTDIR}/evmtrans.lib ${FLAGS}")
		set_target_properties(evmtrans-standalone PROPERTIES STATIC_LIBRARY_FLAGS "${FLAGS}")
		install(TARGETS evmtrans-standalone
				RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
				LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
				ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
	elseif (APPLE)
		add_custom_command(OUTPUT ${EVMTRANS_STANDALONE_FILE}
						   COMMAND libtool -static -o ${EVMTRANS_STANDALONE_FILE} $<TARGET_FILE:evmtrans> ${EVMTRANS_LINK_LIBRARIES}
						   VERBATIM)
		add_custom_target(evmtrans-standalone DEPENDS ${EVMTRANS_STANDALONE_FILE})
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${EVMTRANS_STANDALONE_FILE} DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
	elseif (CMAKE_AR)
		# For platforms using ar a linker scripts is created and used to create
		# the standalone library.

		string(REPLACE ";" "\\naddlib " AR_SCRIPT ";${EVMTRANS_LINK_LIBRARIES}")
		set(AR_SCRIPT "create ${EVMTRANS_STANDALONE_FILE}\\naddlib $<TARGET_FILE:evmtrans>${AR_SCRIPT}\\nsave\\nend\\n")
		set(AR_SCRIPT_FILE ${CMAKE_STATIC_LIBRARY_PREFIX}evmtrans-standalone.ar-script)

		# Generate the linker script.
		add_custom_command(OUTPUT ${AR_SCRIPT_FILE}
				COMMAND printf ${AR_SCRIPT} > ${AR_SCRIPT_FILE}
				DEPENDS $<TARGET_FILE:evmtrans>
				VERBATIM)

		# Execute the script.
		add_custom_command(OUTPUT ${EVMTRANS_STANDALONE_FILE}
				COMMAND ${CMAKE_AR} -M < ${AR_SCRIPT_FILE}
				MAIN_DEPENDENCY ${AR_SCRIPT_FILE}
				VERBATIM)

		add_custom_target(evmtrans-standalone DEPENDS ${EVMTRANS_STANDALONE_FILE})

		# The "thin" library is also provided. It is smaller that the standalone one
		# but cannot be redistributed.
		set(EVMTRANS_STANDALONE_THIN_FILE ${CMAKE_STATIC_LIBRARY_PREFIX}evmtrans-standalone-thin${CMAKE_STATIC_LIBRARY_SUFFIX})

		add_custom_command(OUTPUT ${EVMTRANS_STANDALONE_THIN_FILE}
				COMMAND ${CMAKE_AR} cTq ${EVMTRANS_STANDALONE_THIN_FILE} $<TARGET_FILE:evmtrans> ${EVMTRANS_LINK_LIBRARIES})

		add_custom_target(evmtrans-standalone-thin ALL DEPENDS ${EVMTRANS_STANDALONE_THIN_FILE})
		# FIXME: Looks it will be better to create evmtrans-standalone as a library with costum steps instead of custom taget.
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${EVMTRANS_STANDALONE_FILE} DESTINATION ${CMAKE_INSTALL_LIBDIR} OPTIONAL)
	endif()

	# Export the full path to evmtrans-standalone library to be used in tests.
	# TODO: It would be easier if evmtrans-standalone was a library target, but it
	#       works for now.
	set(EVMTRANS_STANDALONE_LIB ${CMAKE_CURRENT_BINARY_DIR}/${EVMTRANS_STANDALONE_FILE} PARENT_SCOPE)
endif()


# set(Z3_DIR "/usr/local/z3/") # drop

# find_package(Z3 REQUIRED)
# message(STATUS "Z3_FOUND: ${Z3_FOUND}")
# message(STATUS "Found Z3 ${Z3_VERSION_STRING}")
# message(STATUS "Z3_DIR: ${Z3_DIR}")


# # include_directories(${Z3_DIR}/include)
# set(Z3_LIBRARIES ${Z3_DIR}/bin/)
# set(Z3_CXX_INCLUDE_DIRS ${Z3_DIR}/include/)


# Compile standalone evmtrans
add_executable(standalone-evmtrans standalone-evmtrans.cpp ${SOURCES_LIBS})
add_dependencies(standalone-evmtrans LLVM::JIT)
if (LLVM_COMPILE_DEFINITIONS)
	target_compile_definitions(standalone-evmtrans PRIVATE ${LLVM_COMPILE_DEFINITIONS})
endif()
target_include_directories(standalone-evmtrans SYSTEM PRIVATE ${LLVM_INCLUDE_DIRECTORIES})
target_link_libraries(standalone-evmtrans PRIVATE lld::lld LLVM::JIT)


target_include_directories(standalone-evmtrans PRIVATE ${Z3_CXX_INCLUDE_DIRS})
target_link_libraries(standalone-evmtrans PRIVATE ${Z3_LIBRARIES})

install(TARGETS standalone-evmtrans
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
