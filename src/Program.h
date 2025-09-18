#pragma once

struct GLFWwindow;

class Program {
private:
    GLFWwindow* window;
    
public:
    Program();
    ~Program();
    
public:
    auto run() -> int;

private:
    auto terminate() -> void;
};
