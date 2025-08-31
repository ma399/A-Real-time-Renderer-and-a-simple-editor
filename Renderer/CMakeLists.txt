# Renderer Library CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Common library source files
set(COMMON_SOURCES
    common/src/AssimpLoader.cpp
    common/src/CoroutineResourceManager.cpp
    common/src/CoroutineThreadPoolScheduler.cpp
    common/src/EnhancedThreadPool.cpp
    common/src/FileDialog.cpp
    common/src/FileDialogManager.cpp
    common/src/InputManager.cpp
    common/src/Logger.cpp
    common/src/RaycastUtils.cpp
    common/src/STBImage.cpp
    common/src/ThreadPool.cpp
    common/src/TransformManager.cpp
    common/src/Window.cpp
)

set(COMMON_HEADERS
    common/include/AssimpLoader.h
    common/include/CoroutineResourceManager.h
    common/include/CoroutineThreadPoolScheduler.h
    common/include/EnhancedThreadPool.h
    common/include/FileDialog.h
    common/include/FileDialogManager.h
    common/include/InputManager.h
    common/include/LoadingDialog.h
    common/include/Logger.h
    common/include/PriorityTaskQueue.h
    common/include/RaycastUtils.h
    common/include/STBImage.h
    common/include/Task.h
    common/include/TaskPriority.h
    common/include/ThreadPool.h
    common/include/TransformManager.h
    common/include/Window.h
)

# Rendering library source files
set(RENDERING_SOURCES
    rendering/src/Camera.cpp
    rendering/src/Light.cpp
    rendering/src/Material.cpp
    rendering/src/Mesh.cpp
    rendering/src/Model.cpp
    rendering/src/Renderable.cpp
    rendering/src/Renderer.cpp
    rendering/src/Scene.cpp
    rendering/src/Shader.cpp
    rendering/src/ShadowMap.cpp
    rendering/src/Texture.cpp
    rendering/src/Transform.cpp
)

set(RENDERING_HEADERS
    rendering/include/Camera.h
    rendering/include/Light.h
    rendering/include/Material.h
    rendering/include/Mesh.h
    rendering/include/Model.h
    rendering/include/Renderable.h
    rendering/include/Renderer.h
    rendering/include/Scene.h
    rendering/include/Shader.h
    rendering/include/ShadowMap.h
    rendering/include/Texture.h
    rendering/include/Transform.h
)

# Create Renderer static library 
add_library(Renderer STATIC ${COMMON_SOURCES} ${COMMON_HEADERS} ${RENDERING_SOURCES} ${RENDERING_HEADERS})

# Set include directories
target_include_directories(Renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering/include
)

# Link libraries
target_link_libraries(Renderer PUBLIC
    glm::glm
    glfw::glfw
    glad::glad
    imgui::imgui
    stb::stb
    tinyexr::tinyexr
    spdlog::spdlog
    assimp::assimp
    ${OPENGL_LIBRARIES}
)

# Set target properties
set_target_properties(Renderer PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

message(STATUS "Renderer library configured successfully")
