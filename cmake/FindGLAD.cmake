if(NOT TARGET glad::glad)
    include(FetchContent)
    
    message(STATUS "Fetching glad2 via FetchContent...")
    FetchContent_Declare(
        glad2
        GIT_REPOSITORY https://github.com/Dav1dde/glad.git
        GIT_TAG glad2
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(glad2)
    
    # Add GLAD2 cmake directory to load glad_add_library function
    add_subdirectory(${glad2_SOURCE_DIR}/cmake ${CMAKE_BINARY_DIR}/_deps/glad2-build)
    
    # Create OpenGL 4.6 Core library using GLAD2
    glad_add_library(glad_gl_core_46 STATIC REPRODUCIBLE API gl:core=4.6)
    
    # Set wrapper file path to _deps/glad-build directory
    set(GLAD_WRAPPER_DIR "${CMAKE_BINARY_DIR}/_deps/glad-build/include/glad")
    set(GLAD_WRAPPER_FILE "${GLAD_WRAPPER_DIR}/glad.h")
    
    add_custom_command(
        OUTPUT "${GLAD_WRAPPER_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GLAD_WRAPPER_DIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "#ifndef __glad_h_" > "${GLAD_WRAPPER_FILE}"
        COMMAND ${CMAKE_COMMAND} -E echo "#define __glad_h_" >> "${GLAD_WRAPPER_FILE}"
        COMMAND ${CMAKE_COMMAND} -E echo "#include <glad/gl.h>" >> "${GLAD_WRAPPER_FILE}"
        COMMAND ${CMAKE_COMMAND} -E echo "typedef GLADloadfunc GLADloadproc;" >> "${GLAD_WRAPPER_FILE}"
        COMMAND ${CMAKE_COMMAND} -E echo "#define gladLoadGLLoader gladLoadGL" >> "${GLAD_WRAPPER_FILE}"
        COMMAND ${CMAKE_COMMAND} -E echo "#endif // __glad_h_" >> "${GLAD_WRAPPER_FILE}"
        DEPENDS glad_gl_core_46
        COMMENT "Generating GLAD compatibility wrapper"
        VERBATIM
    )
    
    # Create a custom target for the wrapper
    add_custom_target(glad_wrapper_generator ALL DEPENDS "${GLAD_WRAPPER_FILE}")
    
    # Add the wrapper include directory to glad_gl_core_46 target
    target_include_directories(glad_gl_core_46 PUBLIC "${CMAKE_BINARY_DIR}/_deps/glad-build/include")
    
    # Ensure glad::glad alias exists
    if(NOT TARGET glad::glad AND TARGET glad_gl_core_46)
        add_library(glad::glad ALIAS glad_gl_core_46)
    endif()
    
    set(GLAD_FOUND TRUE)
    message(STATUS "GLAD found and configured")
else()
    set(GLAD_FOUND TRUE)
    message(STATUS "GLAD already available")
endif()
