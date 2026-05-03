#include "PrimedGunShared.h"

#include <windows.h>
#include <tlhelp32.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::wofstream g_log;

void Log(std::wstring_view message) {
    if (g_log.is_open()) {
        g_log << message << L"\n";
        g_log.flush();
    }
    std::wcout << message << L"\n";
}

fs::path LocalAppDataPath() {
    wchar_t buffer[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, static_cast<DWORD>(MAX_PATH));
    if (len == 0 || len >= MAX_PATH) {
        return fs::temp_directory_path();
    }
    return fs::path(buffer);
}

std::optional<DWORD> FindProcessIdByName(const wchar_t* exeName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, exeName) == 0) {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return std::nullopt;
}

struct StartedProcess {
    DWORD processId = 0;
    HANDLE process = nullptr;
    HANDLE thread = nullptr;
};

bool IsModuleLoaded(DWORD processId, const fs::path& modulePath) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    const std::wstring wanted = fs::weakly_canonical(modulePath).wstring();
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            std::error_code ec;
            fs::path current = fs::weakly_canonical(entry.szExePath, ec);
            if (!ec && _wcsicmp(current.wstring().c_str(), wanted.c_str()) == 0) {
                CloseHandle(snapshot);
                return true;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return false;
}

std::optional<StartedProcess> StartDolphinSuspended(const fs::path& dolphinPath) {
    if (!fs::exists(dolphinPath)) {
        Log(L"Dolphin path does not exist: " + dolphinPath.wstring());
        return std::nullopt;
    }

    std::wstring commandLine = L"\"" + dolphinPath.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    const fs::path workingDirectory = dolphinPath.parent_path();
    BOOL ok = CreateProcessW(
        dolphinPath.c_str(),
        mutableCommand.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED,
        nullptr,
        workingDirectory.c_str(),
        &startup,
        &process);

    if (!ok) {
        Log(L"CreateProcessW failed for Dolphin.exe");
        return std::nullopt;
    }

    Log(L"Started Dolphin.exe suspended with pid " + std::to_wstring(process.dwProcessId));
    return StartedProcess{ process.dwProcessId, process.hProcess, process.hThread };
}

bool InjectDll(DWORD processId, const fs::path& dllPath) {
    if (IsModuleLoaded(processId, dllPath)) {
        Log(L"Hook DLL is already loaded in Dolphin.");
        return true;
    }

    const std::wstring fullDllPath = fs::weakly_canonical(dllPath).wstring();
    const size_t bytes = (fullDllPath.size() + 1) * sizeof(wchar_t);

    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, processId);
    if (!process) {
        Log(L"OpenProcess failed. Try running the launcher as administrator if Dolphin is elevated.");
        return false;
    }

    void* remotePath = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        Log(L"VirtualAllocEx failed.");
        CloseHandle(process);
        return false;
    }

    BOOL wrote = WriteProcessMemory(process, remotePath, fullDllPath.c_str(), bytes, nullptr);
    if (!wrote) {
        Log(L"WriteProcessMemory failed.");
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryW"));
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remotePath, 0, nullptr);
    if (!thread) {
        Log(L"CreateRemoteThread failed.");
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    WaitForSingleObject(thread, 10000);
    DWORD exitCode = 0;
    GetExitCodeThread(thread, &exitCode);
    CloseHandle(thread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(process);

    if (exitCode == 0) {
        Log(L"LoadLibraryW returned null inside Dolphin.");
        return false;
    }

    Log(L"Injected " + fullDllPath + L" into Dolphin.");
    return true;
}

void EnsureSharedState() {
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(PrimedGun::SharedState), PrimedGun::SharedMemoryName);
    if (!mapping) {
        Log(L"CreateFileMappingW failed for shared state.");
        return;
    }

    auto* state = static_cast<PrimedGun::SharedState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(PrimedGun::SharedState)));
    if (!state) {
        CloseHandle(mapping);
        Log(L"MapViewOfFile failed for shared state.");
        return;
    }

    if (state->magic != PrimedGun::SharedStateMagic || state->version != PrimedGun::SharedStateVersion) {
        *state = PrimedGun::SharedState{};
    }
    state->appHeartbeat++;

    UnmapViewOfFile(state);
    CloseHandle(mapping);
}

fs::path ExeDirectory() {
    wchar_t buffer[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(MAX_PATH));
    return fs::path(buffer).parent_path();
}

std::optional<fs::path> ParseDolphinPath(int argc, wchar_t** argv) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::wstring_view(argv[i]) == L"--dolphin") {
            return fs::path(argv[i + 1]);
        }
    }
    return std::nullopt;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    const fs::path logDir = LocalAppDataPath() / L"PrimedGun";
    std::error_code ec;
    fs::create_directories(logDir, ec);
    g_log.open(logDir / L"PrimedGun_Launcher.log", std::ios::app);

    const fs::path dllPath = ExeDirectory() / L"PrimedGun_DolphinHook.dll";
    if (!fs::exists(dllPath)) {
        Log(L"Missing hook DLL next to launcher: " + dllPath.wstring());
        return 2;
    }

    EnsureSharedState();

    std::optional<DWORD> dolphinPid = FindProcessIdByName(L"Dolphin.exe");
    if (!dolphinPid) {
        std::optional<fs::path> requestedPath = ParseDolphinPath(argc, argv);
        fs::path dolphinPath = requestedPath.value_or(ExeDirectory() / L"Dolphin.exe");
        std::optional<StartedProcess> started = StartDolphinSuspended(dolphinPath);
        if (!started) {
            Log(L"Could not find or start Dolphin.exe.");
            return 3;
        }

        const bool injected = InjectDll(started->processId, dllPath);
        ResumeThread(started->thread);
        CloseHandle(started->thread);
        CloseHandle(started->process);
        Log(L"Resumed Dolphin.exe main thread.");
        return injected ? 0 : 4;
    } else {
        Log(L"Found running Dolphin.exe with pid " + std::to_wstring(*dolphinPid));
    }

    return InjectDll(*dolphinPid, dllPath) ? 0 : 4;
}
