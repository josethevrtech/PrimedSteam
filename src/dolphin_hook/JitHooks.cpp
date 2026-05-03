#include "JitHooks.h"

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace PrimedGun::Hook {
void Log(std::wstring_view message);
}

namespace PrimedGun::Hook::JitHooks {
namespace {

namespace fs = std::filesystem;

struct Anchor {
    const char* text;
    uint32_t maxHits;
};

struct ExecRegion {
    uintptr_t base = 0;
    size_t size = 0;
    DWORD protect = 0;
};

bool g_installed = false;
uint32_t g_pollCount = 0;
uint64_t g_lastDumpToggleMs = 0;
std::vector<ExecRegion> g_lastExecRegions;

constexpr std::array<Anchor, 12> kAnchors{{
    {"JitInterface", 8},
    {"JitBlock", 12},
    {"Jit64", 12},
    {"PowerPC", 12},
    {"PPCSymbol", 8},
    {"SymbolDB", 8},
    {"Debugger", 12},
    {"Gecko", 12},
    {"ActionReplay", 8},
    {"JIT", 12},
    {"Jit", 12},
    {"DBAT", 4},
}};

std::wstring Hex(uintptr_t value) {
    std::wstringstream ss;
    ss << L"0x" << std::hex << std::uppercase << value;
    return ss.str();
}

std::wstring WidenAscii(std::string_view text) {
    std::wstring out;
    out.reserve(text.size());
    for (char c : text) {
        const unsigned char uc = static_cast<unsigned char>(c);
        out.push_back(uc >= 32 && uc <= 126 ? static_cast<wchar_t>(uc) : L'.');
    }
    return out;
}

fs::path LocalAppDataPath() {
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, static_cast<DWORD>(MAX_PATH));
    if (len == 0 || len >= MAX_PATH) {
        return fs::temp_directory_path();
    }
    return fs::path(buffer);
}

bool IsExecProtect(DWORD protect) {
    const DWORD p = protect & 0xff;
    return p == PAGE_EXECUTE ||
        p == PAGE_EXECUTE_READ ||
        p == PAGE_EXECUTE_READWRITE ||
        p == PAGE_EXECUTE_WRITECOPY;
}

bool IsReadableProtect(DWORD protect) {
    const DWORD p = protect & 0xff;
    return p == PAGE_READONLY ||
        p == PAGE_READWRITE ||
        p == PAGE_WRITECOPY ||
        p == PAGE_EXECUTE_READ ||
        p == PAGE_EXECUTE_READWRITE ||
        p == PAGE_EXECUTE_WRITECOPY;
}

void LogImageAnchors(HMODULE module, const MODULEINFO& info) {
    const auto* begin = static_cast<const uint8_t*>(info.lpBaseOfDll);
    const size_t size = static_cast<size_t>(info.SizeOfImage);
    Log(L"JitHooks: Dolphin image base=" + Hex(reinterpret_cast<uintptr_t>(begin)) +
        L" size=" + std::to_wstring(size));

    for (const Anchor& anchor : kAnchors) {
        const auto* needle = reinterpret_cast<const uint8_t*>(anchor.text);
        const size_t needleLen = std::strlen(anchor.text);
        uint32_t hits = 0;

        for (size_t offset = 0; offset + needleLen <= size && hits < anchor.maxHits; ++offset) {
            if (std::memcmp(begin + offset, needle, needleLen) != 0) {
                continue;
            }

            const size_t contextStart = offset > 48 ? offset - 48 : 0;
            const size_t contextEnd = std::min<size_t>(size, offset + needleLen + 96);
            std::string_view context(reinterpret_cast<const char*>(begin + contextStart), contextEnd - contextStart);
            Log(L"JitHooks anchor \"" + WidenAscii(anchor.text) +
                L"\" rva=" + Hex(offset) +
                L" va=" + Hex(reinterpret_cast<uintptr_t>(begin + offset)) +
                L" context=\"" + WidenAscii(context) + L"\"");
            ++hits;
        }

        if (hits == 0) {
            Log(L"JitHooks anchor \"" + WidenAscii(anchor.text) + L"\" not found.");
        }
    }
}

std::vector<ExecRegion> EnumerateExecutablePrivateRegions() {
    std::vector<ExecRegion> regions;
    uintptr_t address = 0;
    MEMORY_BASIC_INFORMATION mbi{};
    while (VirtualQuery(reinterpret_cast<const void*>(address), &mbi, sizeof(mbi)) == sizeof(mbi)) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const size_t size = static_cast<size_t>(mbi.RegionSize);

        if (mbi.State == MEM_COMMIT &&
            mbi.Type == MEM_PRIVATE &&
            IsExecProtect(mbi.Protect) &&
            !(mbi.Protect & PAGE_GUARD) &&
            size >= 0x1000) {
            regions.push_back(ExecRegion{ base, size, mbi.Protect });
        }

        const uintptr_t next = base + size;
        if (next <= address) {
            break;
        }
        address = next;
    }
    return regions;
}

bool SameRegions(const std::vector<ExecRegion>& a, const std::vector<ExecRegion>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].base != b[i].base || a[i].size != b[i].size || a[i].protect != b[i].protect) {
            return false;
        }
    }
    return true;
}

std::wstring HexBytes(const uint8_t* data, size_t size) {
    std::wstringstream ss;
    ss << std::hex << std::uppercase << std::setfill(L'0');
    const size_t count = std::min<size_t>(size, 96);
    for (size_t i = 0; i < count; ++i) {
        if (i != 0) {
            ss << L' ';
        }
        ss << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return ss.str();
}

void LogExecutableRegions(bool force) {
    std::vector<ExecRegion> regions = EnumerateExecutablePrivateRegions();
    if (!force && SameRegions(regions, g_lastExecRegions)) {
        return;
    }
    g_lastExecRegions = regions;

    Log(L"JitHooks executable private regions=" + std::to_wstring(regions.size()));
    const size_t count = std::min<size_t>(regions.size(), 24);
    for (size_t i = 0; i < count; ++i) {
        const ExecRegion& region = regions[i];
        Log(L"  exec[" + std::to_wstring(i) +
            L"] base=" + Hex(region.base) +
            L" size=" + std::to_wstring(region.size) +
            L" protect=" + Hex(region.protect));
    }
}

void DumpExecutableRegions(std::wstring_view reason) {
    const std::vector<ExecRegion> regions = EnumerateExecutablePrivateRegions();
    const uint64_t stamp = GetTickCount64();
    const DWORD pid = GetCurrentProcessId();
    const fs::path dumpDir = LocalAppDataPath() / L"PrimedGun" / L"JitDumps";
    std::error_code ec;
    fs::create_directories(dumpDir, ec);
    if (ec) {
        Log(L"JitHooks dump failed to create directory: " + dumpDir.wstring());
        return;
    }

    const std::wstring prefix = L"jitdump_pid" + std::to_wstring(pid) + L"_" + std::to_wstring(stamp);
    const fs::path manifestPath = dumpDir / (prefix + L".txt");
    std::wofstream manifest(manifestPath);
    if (!manifest.is_open()) {
        Log(L"JitHooks dump failed to open manifest: " + manifestPath.wstring());
        return;
    }

    manifest << L"reason=" << reason << L"\n";
    manifest << L"pid=" << pid << L"\n";
    manifest << L"tick=" << stamp << L"\n";
    manifest << L"regions=" << regions.size() << L"\n";

    Log(L"JitHooks dumping executable private regions reason=" + std::wstring(reason) +
        L" count=" + std::to_wstring(regions.size()) +
        L" manifest=" + manifestPath.wstring());

    for (size_t i = 0; i < regions.size(); ++i) {
        const ExecRegion& region = regions[i];
        const fs::path binPath = dumpDir / (prefix + L"_region" + std::to_wstring(i) + L".bin");

        manifest << L"\nregion[" << i << L"]\n";
        manifest << L"base=" << Hex(region.base) << L"\n";
        manifest << L"size=" << region.size << L"\n";
        manifest << L"protect=" << Hex(region.protect) << L"\n";
        manifest << L"file=" << binPath.filename().wstring() << L"\n";

        if (!IsReadableProtect(region.protect)) {
            manifest << L"status=not-readable\n";
            continue;
        }

        std::ofstream bin(binPath, std::ios::binary);
        if (!bin.is_open()) {
            manifest << L"status=open-failed\n";
            continue;
        }

        const auto* bytes = reinterpret_cast<const char*>(region.base);
        bin.write(bytes, static_cast<std::streamsize>(region.size));
        if (!bin.good()) {
            manifest << L"status=write-failed\n";
            continue;
        }

        const auto* u8 = reinterpret_cast<const uint8_t*>(region.base);
        manifest << L"status=ok\n";
        manifest << L"first_bytes=" << HexBytes(u8, region.size) << L"\n";
        Log(L"  dumped exec[" + std::to_wstring(i) +
            L"] base=" + Hex(region.base) +
            L" size=" + std::to_wstring(region.size) +
            L" file=" + binPath.filename().wstring());
    }
}

} // namespace

bool Install() {
    if (g_installed) {
        return true;
    }
    g_installed = true;

    HMODULE dolphin = GetModuleHandleW(nullptr);
    MODULEINFO info{};
    if (!GetModuleInformation(GetCurrentProcess(), dolphin, &info, sizeof(info))) {
        Log(L"JitHooks: GetModuleInformation failed.");
        return false;
    }

    Log(L"JitHooks installed in passive discovery mode. No JIT/debugger symbols were patched.");
    LogImageAnchors(dolphin, info);
    LogExecutableRegions(true);
    return true;
}

void Poll() {
    if (!g_installed) {
        return;
    }

    ++g_pollCount;
    if ((g_pollCount % 20) == 0) {
        LogExecutableRegions(false);
    }

    const uint64_t nowMs = GetTickCount64();
    const SHORT f6 = GetAsyncKeyState(VK_F6);
    if ((f6 & 0x0001) != 0 && nowMs - g_lastDumpToggleMs > 500) {
        g_lastDumpToggleMs = nowMs;
        DumpExecutableRegions(L"F6");
    }
}

void Shutdown() {
    if (g_installed) {
        Log(L"JitHooks shutdown.");
        g_installed = false;
    }
}

} // namespace PrimedGun::Hook::JitHooks
