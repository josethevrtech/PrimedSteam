#pragma once

#include "PrimedGunShared.h"

#include <windows.h>

namespace PrimedGun::Hook {

class SharedStateView {
public:
    SharedStateView() = default;
    ~SharedStateView();

    SharedStateView(const SharedStateView&) = delete;
    SharedStateView& operator=(const SharedStateView&) = delete;

    bool Open();
    SharedState* Get() const { return state_; }
    void Heartbeat();

private:
    HANDLE mapping_ = nullptr;
    SharedState* state_ = nullptr;
};

} // namespace PrimedGun::Hook
