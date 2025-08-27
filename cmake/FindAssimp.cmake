if(NOT TARGET assimp::assimp)
    include(FetchContent)
    
    message(STATUS "Fetching assimp via FetchContent...")
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.3.1
        GIT_PROGRESS TRUE
    )
    
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    
    # Disable warnings-as-errors for Assimp compilation
    set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    
    # Choose importer types we need
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_DAE_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_3DS_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_BLEND_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_PLY_IMPORTER ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(assimp)
    
    if(NOT TARGET assimp::assimp AND TARGET assimp)
        add_library(assimp::assimp ALIAS assimp)
    endif()
    
    set(ASSIMP_FOUND TRUE)
    message(STATUS "Assimp found and configured")
else()
    set(ASSIMP_FOUND TRUE)
    message(STATUS "Assimp already available")
endif()
