if(NOT TARGET glm::glm)
    include(FetchContent)
    
    message(STATUS "Fetching glm via FetchContent...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(glm)
    
    # Ensure glm::glm alias exists
    if(NOT TARGET glm::glm AND TARGET glm)
        add_library(glm::glm ALIAS glm)
    endif()
    
    set(GLM_FOUND TRUE)
    message(STATUS "GLM found and configured")
else()
    set(GLM_FOUND TRUE)
    message(STATUS "GLM already available")
endif()
