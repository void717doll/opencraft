set(BUILD_SHARED_LIBS OFF CACHE BOOL "")
set(BUILD_STATIC_LIBS ON CACHE BOOL "")

update_git_submodules()

#------------------------------------------------------------------------------
# Libraries
#------------------------------------------------------------------------------
add_subdirectory(entt)
add_subdirectory(glm)
add_subdirectory(lua)
add_subdirectory(sol2)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	include("${CMAKE_SOURCE_DIR}/cmake/sol2_compat.cmake")
endif()
add_subdirectory(tinyxml2)

#------------------------------------------------------------------------------
# - BGFX
#------------------------------------------------------------------------------
set(BGFX_BUILD_TOOLS    ON  CACHE BOOL "" FORCE)
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_INSTALL        OFF CACHE BOOL "" FORCE)
set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)

add_subdirectory(bgfx.cmake)

#------------------------------------------------------------------------------
# - SDL
#------------------------------------------------------------------------------
set(MESSAGE_QUIET ON)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(SDL)
add_subdirectory(SDL_image)

unset(MESSAGE_QUIET)

#------------------------------------------------------------------------------
# - SFML
#------------------------------------------------------------------------------
set(SFML_STATIC TRUE)

set(SFML_BUILD_AUDIO    FALSE CACHE BOOL "" FORCE)
set(SFML_BUILD_GRAPHICS FALSE CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK  FALSE CACHE BOOL "" FORCE)
set(SFML_BUILD_WINDOW   FALSE CACHE BOOL "" FORCE)

add_subdirectory(SFML)

#------------------------------------------------------------------------------
# - zlib
#------------------------------------------------------------------------------
if(APPLE)
	# the bundled zlib mistakes modern macos headers for classic mac os.
	find_package(ZLIB REQUIRED)
	add_library(zlibstatic INTERFACE)
	target_link_libraries(zlibstatic INTERFACE ZLIB::ZLIB)
else()
	add_subdirectory(zlib)
	target_include_directories(zlibstatic PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/zlib)
endif()
