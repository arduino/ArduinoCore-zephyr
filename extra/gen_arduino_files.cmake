# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# get root dir for the project
cmake_path(SET TOP_DIR NORMALIZE ${CMAKE_CURRENT_LIST_DIR}/..)

# get list of variants to be applied
if(CMAKE_ARGC GREATER 3)
	# cmake -P <script> <variant> ...
	foreach(index RANGE 4 ${CMAKE_ARGC})
		math(EXPR index "${index} - 1")
		list(APPEND VARIANTS "${CMAKE_ARGV${index}}")
	endforeach()
	list(TRANSFORM VARIANTS REPLACE "/$" "")
	list(TRANSFORM VARIANTS REPLACE ".*/" "")
else()
	# build for all valid variants
	file(GLOB VARIANTS RELATIVE ${TOP_DIR}/variants variants/*)
	list(REMOVE_ITEM VARIANTS llext linked)
endif()

foreach(variant ${VARIANTS})
	if(NOT IS_DIRECTORY variants/${variant})
		continue()
	endif()

	cmake_path(SET dir ${TOP_DIR}/variants/${variant} NORMALIZE)
	if(NOT EXISTS ${dir}/llext-edk/cmake.cflags)
		continue()
	endif()

	message(STATUS "Processing ${variant}")
	include(${dir}/llext-edk/cmake.cflags)

	# Files forced with -include and system include directories are copied
	# to the EDK, with a joined path inside it: make them relative to the
	# variant, as the include directories.
	# - forced files are found from the variant, which is in the include
	#   path;
	# - system include directories go with the include directories, after
	#   the -iprefix that precedes them in the recipes.
	list(TRANSFORM LLEXT_BASE_CFLAGS REPLACE "^-include${dir}/" "-include")
	set(system_dirs ${LLEXT_BASE_CFLAGS})
	list(FILTER system_dirs INCLUDE REGEX "^-isystem${dir}/")
	list(FILTER LLEXT_BASE_CFLAGS EXCLUDE REGEX "^-isystem${dir}/")
	list(TRANSFORM system_dirs REPLACE "^-isystem${dir}" "-iwithprefix")

	list(TRANSFORM LLEXT_ALL_INCLUDE_CFLAGS REPLACE "-I${dir}" "-iwithprefixbefore")
	list(APPEND LLEXT_ALL_INCLUDE_CFLAGS ${system_dirs})
	list(JOIN LLEXT_ALL_INCLUDE_CFLAGS "\n" EDK_INCLUDES)
	file(WRITE ${dir}/includes.txt "${EDK_INCLUDES}")

	# exclude -imacros entries in platform from the list, make sure no others are present
	list(FILTER LLEXT_BASE_CFLAGS EXCLUDE REGEX "-imacros.*autoconf.h")
	list(FILTER LLEXT_BASE_CFLAGS EXCLUDE REGEX "-imacros.*zephyr_stdint.h")
	set(other_imacros "${LLEXT_BASE_CFLAGS}")
	list(FILTER other_imacros INCLUDE REGEX "-imacros.*")
	if(other_imacros)
		message(FATAL_ERROR "Unexpected -imacros in LLEXT_BASE_CFLAGS: ${other_imacros}")
	endif()

	# exclude other problematic macros shared between C and C++
	list(FILTER LLEXT_BASE_CFLAGS EXCLUDE REGEX "-fdiagnostics-color=always")

	# no absolute EDK path must be left, or the core package cannot be moved
	set(abs_flags ${LLEXT_ALL_INCLUDE_CFLAGS} ${LLEXT_BASE_CFLAGS})
	list(FILTER abs_flags INCLUDE REGEX "${dir}")
	if(abs_flags)
		message(FATAL_ERROR "Absolute EDK paths in the flags of ${variant}: ${abs_flags}")
	endif()

	# get machine flags (-msomething) in a separate list
	set(LLEXT_MACHINE_FLAGS ${LLEXT_BASE_CFLAGS})
	list(FILTER LLEXT_MACHINE_FLAGS INCLUDE REGEX "-m.*")
	list(FILTER LLEXT_BASE_CFLAGS EXCLUDE REGEX "-m.*")

	# (temp) generate C++ flags from C flags
	set(LLEXT_BASE_CXXFLAGS ${LLEXT_BASE_CFLAGS})
	list(FILTER LLEXT_BASE_CXXFLAGS EXCLUDE REGEX "-Wno-pointer-sign")
	list(FILTER LLEXT_BASE_CXXFLAGS EXCLUDE REGEX "-Werror=implicit-int")
	list(FILTER LLEXT_BASE_CXXFLAGS EXCLUDE REGEX "-std=c.*")

	# save flag files
	list(JOIN LLEXT_MACHINE_FLAGS "\n" EDK_MACHINE_FLAGS)
	file(WRITE ${dir}/machine_flags.txt "${EDK_MACHINE_FLAGS}")

	list(JOIN LLEXT_BASE_CFLAGS "\n" EDK_CFLAGS)
	file(WRITE ${dir}/cflags.txt "${EDK_CFLAGS}")

	list(JOIN LLEXT_BASE_CXXFLAGS "\n" EDK_CXXFLAGS)
	file(WRITE ${dir}/cxxflags.txt "${EDK_CXXFLAGS}")
endforeach()
