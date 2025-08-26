#pragma once

#include <string>

struct GLFWwindow;

class Window
{
public:
    Window(int width, int height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    bool should_close() const;
    void swap_buffers() const;
    
    int get_width() const { return width_; }
    int get_height() const { return height_; }
    
    GLFWwindow* get_window_ptr() const { return window_ptr_; }

private:
    GLFWwindow* window_ptr_ = nullptr;
    int width_;
    int height_;
    std::string title_;
};