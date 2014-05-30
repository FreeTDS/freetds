foreach(target mutex1 condition strings)
	add_executable(${target} ${target}.c)
	target_link_libraries(${target} replacements ${lib_BASE})
endforeach(target)
