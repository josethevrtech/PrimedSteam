#include "DolphinXrBridge.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace PrimedGun::Hook {
void Log(std::wstring_view message);
}

namespace PrimedGun::Hook::DolphinXrBridge {
namespace {

constexpr uint32_t kSnapshotSize = 280;
constexpr uint32_t kControllerSize = 116;
constexpr uint32_t kControllerConnectedOffset = 0;
constexpr uint32_t kControllerTriggerOffset = 8;
constexpr uint32_t kControllerStickXOffset = 16;
constexpr uint32_t kControllerAimPoseOffset = 24;
constexpr uint32_t kHeadPoseOffset = 232;
constexpr uint32_t kRuntimeActiveOffset = 264;
constexpr uint32_t kGenerationOffset = 272;
constexpr uint64_t kMaxReasonableGeneration = 1000000000ULL;

uintptr_t g_snapshotAddress = 0;
uintptr_t g_headOnlyAddress = 0;
uint64_t g_lastGeneration = 0;
uint64_t g_lastScanTick = 0;
bool g_loggedMissing = false;
uint64_t g_lastDiagnosticTick = 0;

bool IsReadableWritable(const MEMORY_BASIC_INFORMATION& mbi) {
    if (mbi.State != MEM_COMMIT)
        return false;
    if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS))
        return false;

    const DWORD protect = mbi.Protect & 0xff;
    return protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

float ReadF32(const uint8_t* p, size_t offset) {
    float value = 0.0f;
    std::memcpy(&value, p + offset, sizeof(value));
    return value;
}

uint64_t ReadU64(const uint8_t* p, size_t offset) {
    uint64_t value = 0;
    std::memcpy(&value, p + offset, sizeof(value));
    return value;
}

bool IsFinitePose(const uint8_t* pose) {
    const bool valid = pose[0] != 0;
    if (!valid)
        return false;

    float qx = ReadF32(pose, 16);
    float qy = ReadF32(pose, 20);
    float qz = ReadF32(pose, 24);
    float qw = ReadF32(pose, 28);
    const float len = qx * qx + qy * qy + qz * qz + qw * qw;
    return std::isfinite(len) && len > 0.50f && len < 1.50f;
}

bool IsBoolByte(uint8_t value) {
    return value == 0 || value == 1;
}

bool IsReasonableController(const uint8_t* controller) {
    for (uint32_t offset = 0; offset < 7; ++offset) {
        if (!IsBoolByte(controller[offset]))
            return false;
    }

    const float trigger = ReadF32(controller, kControllerTriggerOffset);
    const float squeeze = ReadF32(controller, kControllerTriggerOffset + 4);
    const float stick_x = ReadF32(controller, kControllerStickXOffset);
    const float stick_y = ReadF32(controller, kControllerStickXOffset + 4);
    if (!std::isfinite(trigger) || trigger < -0.05f || trigger > 1.05f)
        return false;
    if (!std::isfinite(squeeze) || squeeze < -0.05f || squeeze > 1.05f)
        return false;
    if (!std::isfinite(stick_x) || stick_x < -1.20f || stick_x > 1.20f)
        return false;
    if (!std::isfinite(stick_y) || stick_y < -1.20f || stick_y > 1.20f)
        return false;

    return !controller[kControllerConnectedOffset] || IsFinitePose(controller + kControllerAimPoseOffset);
}

bool LooksLikeSnapshot(const uint8_t* p) {
    if (!IsBoolByte(p[kRuntimeActiveOffset]) || p[kRuntimeActiveOffset] == 0)
        return false;

    const uint64_t generation = ReadU64(p, kGenerationOffset);
    if (generation == 0 || generation > kMaxReasonableGeneration)
        return false;

    if (!IsFinitePose(p + kHeadPoseOffset))
        return false;

    const uint8_t* left = p;
    const uint8_t* right = p + kControllerSize;
    if (!IsReasonableController(left) || !IsReasonableController(right))
        return false;
    if (!left[kControllerConnectedOffset] && !right[kControllerConnectedOffset])
        return false;

    return true;
}

bool LooksLikeHeadOnlySnapshot(const uint8_t* p) {
    if (!IsBoolByte(p[kRuntimeActiveOffset]) || p[kRuntimeActiveOffset] == 0)
        return false;

    const uint64_t generation = ReadU64(p, kGenerationOffset);
    if (generation == 0 || generation > kMaxReasonableGeneration)
        return false;

    const uint8_t* left = p;
    const uint8_t* right = p + kControllerSize;
    if (!IsReasonableController(left) || !IsReasonableController(right))
        return false;

    return IsFinitePose(p + kHeadPoseOffset);
}

uintptr_t FindSnapshotCandidate(uint32_t* headOnlyCount, uintptr_t* headOnlyAddress) {
    if (headOnlyCount)
        *headOnlyCount = 0;
    if (headOnlyAddress)
        *headOnlyAddress = 0;

    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    uintptr_t address = reinterpret_cast<uintptr_t>(si.lpMinimumApplicationAddress);
    const uintptr_t maxAddress = reinterpret_cast<uintptr_t>(si.lpMaximumApplicationAddress);

    while (address < maxAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)))
            break;

        const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t end = base + mbi.RegionSize;
        if (IsReadableWritable(mbi) && mbi.RegionSize >= kSnapshotSize) {
            for (uintptr_t p = base; p + kSnapshotSize <= end; p += 8) {
                if (LooksLikeSnapshot(reinterpret_cast<const uint8_t*>(p)))
                    return p;
                if (LooksLikeHeadOnlySnapshot(reinterpret_cast<const uint8_t*>(p))) {
                    if (headOnlyCount)
                        ++(*headOnlyCount);
                    if (headOnlyAddress && *headOnlyAddress == 0)
                        *headOnlyAddress = p;
                }
            }
        }

        address = end;
    }

    return 0;
}

PoseState ReadPoseState(const uint8_t* pose) {
    PoseState out{};
    out.positionMeters.x = ReadF32(pose, 4);
    out.positionMeters.y = ReadF32(pose, 8);
    out.positionMeters.z = ReadF32(pose, 12);
    out.orientation.x = ReadF32(pose, 16);
    out.orientation.y = ReadF32(pose, 20);
    out.orientation.z = ReadF32(pose, 24);
    out.orientation.w = ReadF32(pose, 28);
    return out;
}

PoseState ReadControllerAimPose(const uint8_t* controller) {
    return ReadPoseState(controller + kControllerAimPoseOffset);
}

void CopySnapshot(SharedState* state, const uint8_t* snapshot) {
    const uint64_t generation = ReadU64(snapshot, kGenerationOffset);
    if (generation == g_lastGeneration)
        return;

    const uint8_t* left = snapshot;
    const uint8_t* right = snapshot + kControllerSize;

    state->leftHandPose = ReadControllerAimPose(left);
    state->leftHandPose.linearVelocityMetersPerSecond.x = ReadF32(left, kControllerTriggerOffset);
    state->leftHandPose.linearVelocityMetersPerSecond.y = ReadF32(left, kControllerStickXOffset);
    state->leftHandPose.linearVelocityMetersPerSecond.z = ReadF32(left, kControllerStickXOffset + 4);

    state->rightHandPose = ReadControllerAimPose(right);
    state->rightHandPose.linearVelocityMetersPerSecond.x = ReadF32(right, kControllerTriggerOffset);
    state->rightHandPose.linearVelocityMetersPerSecond.y = ReadF32(right, kControllerStickXOffset);
    state->rightHandPose.linearVelocityMetersPerSecond.z = ReadF32(right, kControllerStickXOffset + 4);

    state->hmdPose = ReadPoseState(snapshot + kHeadPoseOffset);
    state->trackingSource = 1;
    state->trackingRuntimeActive = 1;
    state->trackingGeneration = generation;
    g_lastGeneration = generation;
}

void CopyHeadOnlySnapshot(SharedState* state, const uint8_t* snapshot) {
    const uint64_t generation = ReadU64(snapshot, kGenerationOffset);
    if (generation == g_lastGeneration)
        return;

    state->hmdPose = ReadPoseState(snapshot + kHeadPoseOffset);
    state->trackingSource = 2;
    state->trackingRuntimeActive = 1;
    state->trackingGeneration = generation;
    g_lastGeneration = generation;
}

} // namespace

void Poll(SharedState* state) {
    if (!state)
        return;

    const uint64_t now = GetTickCount64();
    const bool full_ok =
        g_snapshotAddress && LooksLikeSnapshot(reinterpret_cast<const uint8_t*>(g_snapshotAddress));
    const bool head_only_ok =
        g_headOnlyAddress &&
        LooksLikeHeadOnlySnapshot(reinterpret_cast<const uint8_t*>(g_headOnlyAddress));

    if (!full_ok && !head_only_ok) {
        if (now - g_lastScanTick < 2000)
            return;

        g_lastScanTick = now;
        uint32_t headOnlyCount = 0;
        g_headOnlyAddress = 0;
        g_snapshotAddress = FindSnapshotCandidate(&headOnlyCount, &g_headOnlyAddress);
        if (g_snapshotAddress) {
            g_loggedMissing = false;
            Log(L"DolphinXR OpenXR input snapshot found at 0x" +
                std::to_wstring(static_cast<unsigned long long>(g_snapshotAddress)));
        } else if (g_headOnlyAddress) {
            g_loggedMissing = false;
            Log(L"DolphinXR OpenXR HMD-only snapshot found at 0x" +
                std::to_wstring(static_cast<unsigned long long>(g_headOnlyAddress)) +
                L" candidates=" + std::to_wstring(headOnlyCount));
        } else if (!g_loggedMissing) {
            g_loggedMissing = true;
            Log(L"DolphinXR OpenXR input snapshot not active yet. head-only candidates=" +
                std::to_wstring(headOnlyCount));
        } else if (now - g_lastDiagnosticTick >= 10000) {
            g_lastDiagnosticTick = now;
            Log(L"DolphinXR OpenXR input scan: head-only candidates=" +
                std::to_wstring(headOnlyCount));
        }
    }

    if (g_snapshotAddress && LooksLikeSnapshot(reinterpret_cast<const uint8_t*>(g_snapshotAddress)))
        CopySnapshot(state, reinterpret_cast<const uint8_t*>(g_snapshotAddress));
    else if (g_headOnlyAddress &&
             LooksLikeHeadOnlySnapshot(reinterpret_cast<const uint8_t*>(g_headOnlyAddress)))
        CopyHeadOnlySnapshot(state, reinterpret_cast<const uint8_t*>(g_headOnlyAddress));
}

} // namespace PrimedGun::Hook::DolphinXrBridge
