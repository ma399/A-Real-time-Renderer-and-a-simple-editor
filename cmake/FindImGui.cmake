if(NOT TARGET imgui::imgui)
    include(FetchContent)
    
    message(STATUS "Fetching imgui via FetchContent...")
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.90.9
        GIT_PROGRESS TRUE
    )
    FetchContent_Populate(imgui)
    
    if(NOT TARGET imgui)
        set(IMGUI_SOURCES
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )
        
        add_library(imgui STATIC ${IMGUI_SOURCES})
        target_include_directories(imgui PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
        )
        target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    endif()
    
    if(TARGET imgui AND TARGET glad::glad AND TARGET glfw::glfw)
        target_link_libraries(imgui PUBLIC glfw::glfw glad::glad ${OPENGL_LIBRARIES})
    endif()
    
    if(NOT TARGET imgui::imgui AND TARGET imgui)
        add_library(imgui::imgui ALIAS imgui)
    endif()
    
    set(IMGUI_FOUND TRUE)
    message(STATUS "ImGui found and configured")
else()
    set(IMGUI_FOUND TRUE)
    message(STATUS "ImGui already available")
endif()
