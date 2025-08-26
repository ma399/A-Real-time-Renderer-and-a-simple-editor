#include "Window.h"
#include <stdexcept> 

#include <GLFW/glfw3.h>

Window::Window(int width, int height, const std::string& title)
    : width_(width), height_(height), title_(title)
{
    window_ptr_ = glfwCreateWindow(width_, height_, title_.c_str(), NULL, NULL);
    if (!window_ptr_)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_ptr_);
}

Window::~Window()
{
    if (window_ptr_)
    {
        glfwDestroyWindow(window_ptr_);
    }
}

Window::Window(Window&& other) noexcept
    : window_ptr_(other.window_ptr_), width_(other.width_), height_(other.height_), title_(std::move(other.title_))
{
    other.window_ptr_ = nullptr;
}

Window& Window::operator=(Window&& other) noexcept
{
    if (this != &other)
    {
        if (window_ptr_) {
            glfwDestroyWindow(window_ptr_);
        }
        
        window_ptr_ = other.window_ptr_;
        width_ = other.width_;
        height_ = other.height_;
        title_ = std::move(other.title_);

        other.window_ptr_ = nullptr;
    }
    return *this;
}

bool Window::should_close() const
{
    return glfwWindowShouldClose(window_ptr_);
}

void Window::swap_buffers() const
{
    glfwSwapBuffers(window_ptr_);
}