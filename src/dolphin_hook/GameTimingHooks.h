#pragma once

namespace PrimedGun::Hook::GameTimingHooks {

bool Install();
void SuppressLockCameraPitchForLogicTick();
void Poll();
void Shutdown();

} // namespace PrimedGun::Hook::GameTimingHooks
