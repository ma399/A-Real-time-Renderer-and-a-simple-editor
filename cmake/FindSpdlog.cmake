if(NOT TARGET spdlog::spdlog)
    include(FetchContent)
    
    message(STATUS "Fetching spdlog via FetchContent...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(spdlog)
    
    target_compile_definitions(spdlog PUBLIC SPDLOG_USE_STD_FORMAT)
    # Ensure C++20 is enabled for std::format support
    target_compile_features(spdlog PUBLIC cxx_std_20)


    if(NOT TARGET spdlog::spdlog AND TARGET spdlog)
        add_library(spdlog::spdlog ALIAS spdlog)
    endif()
    
    set(SPDLOG_FOUND TRUE)
    message(STATUS "Spdlog found and configured")
else()
    set(SPDLOG_FOUND TRUE)
    message(STATUS "Spdlog already available")
endif()
