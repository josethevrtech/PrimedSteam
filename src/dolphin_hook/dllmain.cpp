#include "HookRuntime.h"

#include <windows.h>

extern "C" __declspec(dllexport) BOOL PrimedGun_InstallHooks() {
    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        PrimedGun::Hook::StartRuntime(module);
        break;
    case DLL_PROCESS_DETACH:
        PrimedGun::Hook::StopRuntime();
        break;
    default:
        break;
    }
    return TRUE;
}
