function (make_demo NAME)
	add_executable(${NAME} ${NAME}.cc)
	target_link_libraries(${NAME} hulaloop::hulaloop)
endfunction()

make_demo(signal_handler)
