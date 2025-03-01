find_package(Catch2 3 REQUIRED)

file(GLOB sources "*.cc")

add_executable(hulaloop_tests ${sources})

target_link_libraries(hulaloop_tests PRIVATE
	Catch2::Catch2WithMain
	hulaloop::hulaloop
)

include(Catch)
catch_discover_tests(hulaloop_tests
	ADD_TAGS_AS_LABELS
)
