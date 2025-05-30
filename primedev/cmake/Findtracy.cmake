if(NOT tracy_FOUND)
    check_init_submodule(${PROJECT_SOURCE_DIR}/primedev/thirdparty/tracy)

	add_subdirectory(${PROJECT_SOURCE_DIR}/primedev/thirdparty/tracy tracy)
    set(tracy_FOUND 1)
endif()
