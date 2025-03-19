cmake_minimum_required(VERSION 3.12)

project(hulaloop
    VERSION 0.0.3
		DESCRIPTION "C++ event loop"
    HOMEPAGE_URL "https://github.com/shnupta/hulaloop")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

function(hulamessage ARGS)
	message("hulaloop => " ${ARGS})
endfunction()

function(hulaerror ARGS)
	message(FATAL_ERROR "hulaloop => " ${ARGS})
endfunction()

add_compile_options(-Wall)

set(HULALOOP_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
include_directories(${HULALOOP_INCLUDE_DIR})

option(HULA_DEMOS BOOL OFF)
option(HULA_COVERAGE BOOL OFF)
option(HULA_SANITIZE BOOL OFF)

if (HULA_SANITIZE AND HULA_COVERAGE)
	hulaerror("specifying both HULA_SANITIZE and HULA_COVERAGE is not supported")
endif()

if (HULA_COVERAGE)
	hulamessage("coverage build")
	add_compile_options(-g -O0 -fprofile-arcs -ftest-coverage)
	add_link_options(-fprofile-arcs -ftest-coverage)
endif()

if (HULA_SANITIZE)
	hulamessage("sanitize build")
	add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address,undefined)
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
	hulamessage("building tests")
	add_subdirectory(tests)
endif()

if (HULA_DEMOS)
	hulamessage("building demos")
	add_subdirectory(demos)
endif()
