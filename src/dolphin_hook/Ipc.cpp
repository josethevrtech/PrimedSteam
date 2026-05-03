#include "Ipc.h"

namespace PrimedGun::Hook {

SharedStateView::~SharedStateView() {
    if (state_) {
        UnmapViewOfFile(state_);
    }
    if (mapping_) {
        CloseHandle(mapping_);
    }
}

bool SharedStateView::Open() {
    mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedState), SharedMemoryName);
    if (!mapping_) {
        return false;
    }

    state_ = static_cast<SharedState*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedState)));
    if (!state_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
        return false;
    }

    if (state_->magic != SharedStateMagic || state_->version != SharedStateVersion) {
        *state_ = SharedState{};
    }

    return true;
}

void SharedStateView::Heartbeat() {
    if (state_) {
        state_->hookHeartbeat++;
    }
}

} // namespace PrimedGun::Hook
