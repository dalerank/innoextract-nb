# Fetch and build compression dependencies when system packages are missing.
# Requires CMake 3.14+ (FetchContent_MakeAvailable).
#
# Sets the same variables as find_package for consumers:
#   LZMA_FOUND / LZMA_LIBRARIES / LZMA_INCLUDE_DIR / LZMA_DEFINITIONS
#   ZLIB_FOUND / ZLIB_LIBRARIES / ZLIB_INCLUDE_DIR
#   BZIP2_FOUND / BZIP2_LIBRARIES / BZIP2_INCLUDE_DIR

include(FetchContent)

# Upstream bzip2 has no CMakeLists.txt; allow Populate without MakeAvailable (CMP0169).
if(POLICY CMP0169)
	cmake_policy(SET CMP0169 OLD)
endif()

set(INNOEXTRACT_XZ_TAG "v5.6.4" CACHE STRING "xz-utils git tag when fetching liblzma")
set(INNOEXTRACT_ZLIB_TAG "v1.3.1" CACHE STRING "zlib git tag when fetching")
set(INNOEXTRACT_BZIP2_REPO "https://gitlab.com/bzip2/bzip2.git" CACHE STRING
	"bzip2 git repository when fetching")
set(INNOEXTRACT_BZIP2_TAG "bzip2-1.0.8" CACHE STRING "bzip2 git tag when fetching")

function(innoextract_fetch_lzma)
	message(STATUS "innoextract: liblzma not found — fetching xz-utils ${INNOEXTRACT_XZ_TAG}")
	set(_old_build_shared "${BUILD_SHARED_LIBS}")
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(XZ_NLS OFF CACHE BOOL "" FORCE)
	set(XZ_DOXYGEN OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_XZ OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_XZDEC OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_LZMADEC OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_LZMAINFO OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_XZDIFF OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_XZGREP OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_XZLESS OFF CACHE BOOL "" FORCE)
	set(XZ_TOOL_XZMORE OFF CACHE BOOL "" FORCE)
	set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(innoextract_xz
		GIT_REPOSITORY https://github.com/tukaani-project/xz.git
		GIT_TAG ${INNOEXTRACT_XZ_TAG}
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(innoextract_xz)
	if(NOT TARGET liblzma)
		message(FATAL_ERROR "innoextract: fetched xz-utils but liblzma target is missing")
	endif()
	set(LZMA_LIBRARIES liblzma PARENT_SCOPE)
	set(LZMA_INCLUDE_DIR "" PARENT_SCOPE)
	set(LZMA_DEFINITIONS "" PARENT_SCOPE)
	set(LZMA_FOUND TRUE PARENT_SCOPE)
	if(DEFINED _old_build_shared)
		set(BUILD_SHARED_LIBS "${_old_build_shared}" CACHE BOOL "" FORCE)
	endif()
endfunction()

function(innoextract_fetch_zlib)
	message(STATUS "innoextract: zlib not found — fetching zlib ${INNOEXTRACT_ZLIB_TAG}")
	set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
	FetchContent_Declare(innoextract_zlib
		GIT_REPOSITORY https://github.com/madler/zlib.git
		GIT_TAG ${INNOEXTRACT_ZLIB_TAG}
		GIT_SHALLOW TRUE
	)
	FetchContent_MakeAvailable(innoextract_zlib)
	if(TARGET zlibstatic)
		set(_zlib_target zlibstatic)
	elseif(TARGET zlib)
		set(_zlib_target zlib)
	elseif(TARGET ZLIB::ZLIB)
		set(_zlib_target ZLIB::ZLIB)
	else()
		message(FATAL_ERROR "innoextract: fetched zlib but no usable library target was created")
	endif()
	set(ZLIB_LIBRARIES ${_zlib_target} PARENT_SCOPE)
	set(ZLIB_INCLUDE_DIR
		"${innoextract_zlib_SOURCE_DIR}"
		"${innoextract_zlib_BINARY_DIR}"
		PARENT_SCOPE
	)
	set(ZLIB_FOUND TRUE PARENT_SCOPE)
endfunction()

function(innoextract_fetch_bzip2)
	message(STATUS "innoextract: bzip2 not found — fetching ${INNOEXTRACT_BZIP2_TAG}")
	FetchContent_Declare(innoextract_bzip2
		GIT_REPOSITORY ${INNOEXTRACT_BZIP2_REPO}
		GIT_TAG ${INNOEXTRACT_BZIP2_TAG}
		GIT_SHALLOW TRUE
	)
	# Upstream bzip2 has no CMakeLists; Populate + manual static lib (CMP0169 OLD above).
	FetchContent_GetProperties(innoextract_bzip2)
	if(NOT innoextract_bzip2_POPULATED)
		FetchContent_Populate(innoextract_bzip2)
	endif()
	if(NOT TARGET innoextract_bz2)
		add_library(innoextract_bz2 STATIC
			${innoextract_bzip2_SOURCE_DIR}/blocksort.c
			${innoextract_bzip2_SOURCE_DIR}/huffman.c
			${innoextract_bzip2_SOURCE_DIR}/crctable.c
			${innoextract_bzip2_SOURCE_DIR}/randtable.c
			${innoextract_bzip2_SOURCE_DIR}/compress.c
			${innoextract_bzip2_SOURCE_DIR}/decompress.c
			${innoextract_bzip2_SOURCE_DIR}/bzlib.c
		)
		target_include_directories(innoextract_bz2 PUBLIC ${innoextract_bzip2_SOURCE_DIR})
		# Avoid pulling in bzip2's stdio helpers into a library-only build.
		target_compile_definitions(innoextract_bz2 PRIVATE BZ_NO_STDIO)
	endif()
	set(BZIP2_LIBRARIES innoextract_bz2 PARENT_SCOPE)
	set(BZIP2_INCLUDE_DIR ${innoextract_bzip2_SOURCE_DIR} PARENT_SCOPE)
	set(BZIP2_FOUND TRUE PARENT_SCOPE)
	set(BZip2_FOUND TRUE PARENT_SCOPE)
endfunction()
