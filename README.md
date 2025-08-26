# The Real-time Renderer and the Simple Editor

A real-time renderer demo project built with modern C++20 and OpenGL.

## Language Versions

- [中文版本 ](README.CN.md)
- [English Version](README.EN.md)

## Quick Start

```bash
# Clone the repository
git clone https://github.com/ma399/A-Real-time-Renderer-and-a-simple-editor.git
cd A-Real-time-Renderer-and-a-simple-editor

# Build and run
mkdir build && cd build

# Windows (Visual Studio)
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
.\bin\Release\Application.exe

# Windows (MinGW + Ninja)
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
ninja
.\bin\Application.exe
```

## Features

- Modern C++20 with coroutines
- Real-time rendering with OpenGL 4.3+
- Deferred shading pipeline
- PBR (Physically Based Rendering)
- SSGI (Screen Space Global Illumination)
- ImGui-based editor interface
- Cross-platform support (Windows only)
