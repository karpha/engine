#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include <iostream>

class Window{
public:
    Window(int Width, int Height, std::string &windowName);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void initWindow();
    static void framebufferResizeCallback(GLFWwindow* window, int Width, int Height);
    
    
    GLFWwindow* getWindow();
    int getWindowWidth();   // 对于私有成员，有get()，同时也不要忘记set()
    int getWindowHeight();
    bool getFrameBufferResized(){
        return frameBufferResized;
    }
    std::string &getWindowName(){
        return windowName;
    }
    void setWindowWidth(int width){
        Width = width;
    }
    void setWindowHeight(int height){
        Height = height;
    }
    void setWindowName(std::string& name){
        windowName = name;
    }
    void setFrameBufferResized(bool resized){
        frameBufferResized = resized;
    }
    
private:
    GLFWwindow* window;
    std::string windowName;
    int Width;
    int Height;
    bool frameBufferResized = false;
};