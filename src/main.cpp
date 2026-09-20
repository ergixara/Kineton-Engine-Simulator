#include "../include/engine_sim_application.h"

#include <iostream>
#include <string>

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    (void)nCmdShow;
    (void)lpCmdLine;
    (void)hPrevInstance;

    EngineSimApplication application;
    const bool verify3d = std::string(lpCmdLine).find("--verify-3d") != std::string::npos;
    application.setVisualCheck(verify3d);
    application.initialize((void *)&hInstance, ysContextObject::DeviceAPI::DirectX11);
    application.run();
    application.destroy();

    return verify3d && !application.visualCheckPassed() ? 1 : 0;
}
