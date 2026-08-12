#include "window.h"
// #include "application.h"
Window::Window(int Width, int Height, std::string &windowName){
    setWindowHeight(Height);
    setWindowWidth(Width);
    setWindowName(windowName);
    // RAII：构造函数中完成资源（GLFW 窗口）的创建
    initWindow();
}

Window::~Window(){
    if(window){
        glfwDestroyWindow(window);
    }
    glfwTerminate();
}

GLFWwindow* Window::getWindow(){
    return window;
}

int Window::getWindowHeight(){
    return Height;
}

int Window::getWindowWidth(){
    return Width;
}

void Window::initWindow(){
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(getWindowWidth(), getWindowHeight(), getWindowName().c_str(), nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, Window::framebufferResizeCallback);
}

void Window::framebufferResizeCallback(GLFWwindow* window, int Width, int Height){
    Window* pWindow = reinterpret_cast<Window*>(glfwGetWindowUserPointer(window));
    // app->getFrameBufferResized() = true;
    pWindow->setFrameBufferResized(true);
}