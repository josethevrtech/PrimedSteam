#pragma once

namespace PrimedGun::Hook::VulkanHooks {

bool InstallIfAvailable();
void PollRuntimeControls();
void Shutdown();

} // namespace PrimedGun::Hook::VulkanHooks
