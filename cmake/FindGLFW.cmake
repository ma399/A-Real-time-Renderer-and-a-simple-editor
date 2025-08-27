if(NOT TARGET glfw::glfw)
    include(FetchContent)
    
    message(STATUS "Fetching glfw via FetchContent...")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.9
        GIT_PROGRESS TRUE
    )
    
    # Prevent building glfw examples/tests/docs
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(glfw)
    
    # Upstream creates target glfw; ensure alias
    if(NOT TARGET glfw::glfw AND TARGET glfw)
        add_library(glfw::glfw ALIAS glfw)
    endif()
    
    set(GLFW_FOUND TRUE)
    message(STATUS "GLFW found and configured")
else()
    set(GLFW_FOUND TRUE)
    message(STATUS "GLFW already available")
endif()
