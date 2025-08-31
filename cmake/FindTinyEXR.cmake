if(NOT TARGET tinyexr::tinyexr)
    include(FetchContent)
    
    message(STATUS "Fetching tinyexr via FetchContent...")
    FetchContent_Declare(
        tinyexr
        GIT_REPOSITORY https://github.com/syoyo/tinyexr.git
        GIT_TAG v1.0.8
        GIT_PROGRESS TRUE
    )
    FetchContent_Populate(tinyexr)
    
    if(NOT TARGET tinyexr)
        # Create miniz library first
        add_library(miniz STATIC ${tinyexr_SOURCE_DIR}/deps/miniz/miniz.c)
        target_include_directories(miniz PUBLIC ${tinyexr_SOURCE_DIR}/deps/miniz)
        
        # Create tinyexr interface library
        add_library(tinyexr INTERFACE)
        target_include_directories(tinyexr INTERFACE 
            ${tinyexr_SOURCE_DIR}
            ${tinyexr_SOURCE_DIR}/deps/miniz
        )
        target_compile_definitions(tinyexr INTERFACE TINYEXR_IMPLEMENTATION)
        target_link_libraries(tinyexr INTERFACE miniz)
    endif()
    
    if(NOT TARGET tinyexr::tinyexr AND TARGET tinyexr)
        add_library(tinyexr::tinyexr ALIAS tinyexr)
    endif()
    
    set(TINYEXR_FOUND TRUE)
    message(STATUS "TinyEXR found and configured")
else()
    set(TINYEXR_FOUND TRUE)
    message(STATUS "TinyEXR already available")
endif()
