# Application CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Find ImGui using custom find module
find_package(ImGui REQUIRED)

# Application source files
set(APPLICATION_SOURCES
    main.cpp
    application.cpp
    GUI.cpp
)

set(APPLICATION_HEADERS
    Application.h
    GUI.h
)

# Create Application executable
add_executable(Application ${APPLICATION_SOURCES} ${APPLICATION_HEADERS})

# Set include directories
target_include_directories(Application PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(Application PRIVATE
    Renderer
    imgui::imgui
    ${OPENGL_LIBRARIES}
)

# Set target properties
set_target_properties(Application PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Make assets dependency
add_dependencies(Application copy_assets)

message(STATUS "Application executable configured successfully")
