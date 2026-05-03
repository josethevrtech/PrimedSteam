#pragma once

#include <windows.h>

namespace PrimedGun::Hook {

bool StartRuntime(HMODULE module);
void StopRuntime();

} // namespace PrimedGun::Hook
