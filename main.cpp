#include <iostream>

#include "base/application.hpp"


class SandboxApplication : public vk_sandbox::Application  {
public:
    SandboxApplication(const std::string& title) : Application(title) {}
    void draw() override {
    }
};



int main() {
    SandboxApplication application("sandbox");
    application.init();
    application.destroy();
    return 0;
}
