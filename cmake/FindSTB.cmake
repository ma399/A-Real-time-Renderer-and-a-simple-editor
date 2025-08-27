if(NOT TARGET stb::stb)
    include(FetchContent)
    
    message(STATUS "Fetching stb via FetchContent...")
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_PROGRESS TRUE
    )
    FetchContent_Populate(stb)
    
    if(NOT TARGET stb)
        add_library(stb INTERFACE)
        target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
    endif()
    
    if(NOT TARGET stb::stb AND TARGET stb)
        add_library(stb::stb ALIAS stb)
    endif()
    
    set(STB_FOUND TRUE)
    message(STATUS "STB found and configured")
else()
    set(STB_FOUND TRUE)
    message(STATUS "STB already available")
endif()
