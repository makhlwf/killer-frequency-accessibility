#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <filesystem>
#include <iostream>
namespace fs = std::filesystem;
int wmain(int argc, wchar_t **argv) {
    wchar_t executable[MAX_PATH];
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    fs::path folder = fs::path(executable).parent_path();
    fs::path root = folder.parent_path().parent_path();
    fs::path dll = folder / L"KillerFrequencyAccess.dll";
    bool explicitLibrary = argc == 4 && std::wstring(argv[1]) == L"--load";
    fs::path music = folder / L"KillerFrequencyMusic.dll";
    if (!explicitLibrary && !fs::exists(music)) {
        MessageBoxW(
            nullptr,
            L"The matched music and desk-controls library is missing. Reinstall the complete accessibility package.",
            L"Killer Frequency accessibility", MB_ICONERROR);
        return 7;
    }

    if (explicitLibrary)
        dll = fs::path(argv[3]);
    if (!fs::exists(dll) || !fs::exists(root / L"KillerFrequency.exe")) {
        MessageBoxW(
            nullptr,
            L"Keep this launcher in AccessibilityMod\\bin beside KillerFrequencyAccess.dll.",
            L"Killer Frequency accessibility", MB_ICONERROR);
        return 1;
    }
    DWORD pid = 0;
    if (explicitLibrary || (argc == 3 && std::wstring(argv[1]) == L"--attach"))
        pid = wcstoul(argv[2], nullptr, 10);
    else {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32W pe{sizeof(pe)};
        if (Process32FirstW(snap, &pe))
            do {
                if (_wcsicmp(pe.szExeFile, L"KillerFrequency.exe") == 0) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        CloseHandle(snap);
    }
    if (!pid) {
        std::wstring command = L"\"" + (root / L"KillerFrequency.exe").wstring() + L"\"";
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                            root.c_str(), &si, &pi)) {
            MessageBoxW(nullptr, L"Could not start the game.", L"Killer Frequency accessibility",
                        MB_ICONERROR);
            return 2;
        }
        pid = pi.dwProcessId;
        CloseHandle(pi.hThread);
        WaitForInputIdle(pi.hProcess, 15000);
        // Unity loads its window before IL2CPP and its GC finish initialization.
        Sleep(5000);
        CloseHandle(pi.hProcess);
    }
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                     PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        MessageBoxW(
            nullptr,
            L"Cannot open the game process. Run the game and launcher at the same permission level.",
            L"Killer Frequency accessibility", MB_ICONERROR);
        return 3;
    }
    auto path = dll.wstring();
    SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
    void *remote =
        VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(process, remote, path.c_str(), bytes, nullptr)) {
        auto error = GetLastError();
        if (remote)
            VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        auto message =
            L"Could not copy the accessibility library path into the game. Windows error " +
            std::to_wstring(error);
        MessageBoxW(nullptr, message.c_str(), L"Killer Frequency accessibility", MB_ICONERROR);
        return 4;
    }
    // Resolve LoadLibraryW in its owning module, then use that module's base in the target.
    auto load = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE owner{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(load), &owner);
    wchar_t ownerPath[MAX_PATH];
    GetModuleFileNameW(owner, ownerPath, MAX_PATH);
    uintptr_t targetBase = 0;
    for (int tries = 0; tries < 100 && !targetBase; ++tries) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        MODULEENTRY32W me{sizeof(me)};
        if (snap != INVALID_HANDLE_VALUE) {
            if (Module32FirstW(snap, &me))
                do {
                    if (_wcsicmp(me.szModule, fs::path(ownerPath).filename().c_str()) == 0) {
                        targetBase = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                        break;
                    }
                } while (Module32NextW(snap, &me));
            CloseHandle(snap);
        }
        if (!targetBase)
            Sleep(100);
    }
    auto entry = reinterpret_cast<LPTHREAD_START_ROUTINE>(
        targetBase + (reinterpret_cast<uintptr_t>(load) - reinterpret_cast<uintptr_t>(owner)));
    HANDLE thread =
        targetBase ? CreateRemoteThread(process, nullptr, 0, entry, remote, 0, nullptr) : nullptr;
    if (!thread) {
        auto error = GetLastError();
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        auto message = L"Could not load the accessibility library into the game. Windows error " +
                       std::to_wstring(error);
        MessageBoxW(nullptr, message.c_str(), L"Killer Frequency accessibility", MB_ICONERROR);
        return 5;
    }
    DWORD wait = WaitForSingleObject(thread, 15000), result = 0;
    if (wait == WAIT_OBJECT_0) {
        GetExitCodeThread(thread, &result);
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    }
    CloseHandle(thread);
    CloseHandle(process);
    if (wait != WAIT_OBJECT_0 || !result) {
        MessageBoxW(
            nullptr,
            L"The accessibility library did not load. See AccessibilityMod\\bin\\accessibility.log.",
            L"Killer Frequency accessibility", MB_ICONERROR);
        return 6;
    }
    if (!explicitLibrary && fs::exists(music)) {
        std::wstring cmd = L"\"" + std::wstring(executable) + L"\" --load " + std::to_wstring(pid) +
                           L" \"" + music.wstring() + L"\"";
        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                           root.c_str(), &si, &pi)) {
            DWORD childWait = WaitForSingleObject(pi.hProcess, 20000), childExit = 1;
            if (childWait == WAIT_OBJECT_0)
                GetExitCodeProcess(pi.hProcess, &childExit);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (childWait != WAIT_OBJECT_0 || childExit != 0) {
                MessageBoxW(
                    nullptr,
                    L"The desk-controls library did not load. Check accessibility.log and music-fix.log, then reinstall a matched package.",
                    L"Killer Frequency accessibility", MB_ICONERROR);
                return 8;
            }
        } else {
            MessageBoxW(nullptr, L"Music accessibility support could not start.",
                        L"Killer Frequency accessibility", MB_ICONERROR);
            return 7;
        }
    }
    return 0;
}
