cmake_minimum_required(VERSION 3.12)

project(hulaloop
    VERSION 0.0.1
		DESCRIPTION "C++ event loop"
    HOMEPAGE_URL "https://github.com/shnupta/hulaloop")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(HULALOOP_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
include_directories(${HULALOOP_INCLUDE_DIR})

option(HULA_BUILD_DEMOS BOOL OFF)
option(HULA_COVERAGE BOOL OFF)

if (HULA_COVERAGE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

add_library(hulaloop INTERFACE)
add_library(hulaloop::hulaloop ALIAS hulaloop)

set(_hulaloop_is_toplevel_project FALSE)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	set(_hulaloop_is_toplevel_project TRUE)
endif()

target_include_directories(
    hulaloop
    INTERFACE
		$<BUILD_INTERFACE:${HULALOOP_INCLUDE_DIR}>
		$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_definitions(hulaloop INTERFACE "")

if (_hulaloop_is_toplevel_project)
	include(CTest)
	message("hulaloop => building tests")
	add_subdirectory(tests)
endif()

if (HULA_BUILD_DEMOS)
	message("hulaloop => building demos")
	add_subdirectory(demos)
endif()
