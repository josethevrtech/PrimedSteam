#pragma once

#include "PrimedGunShared.h"

namespace PrimedGun::Hook::OpenXrHooks {

bool InstallIfAvailable(SharedState* state);
void Poll(SharedState* state);
void Shutdown();

} // namespace PrimedGun::Hook::OpenXrHooks
