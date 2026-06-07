#include <windows.h>
#include <string>

DWORD WINAPI PulseThread(LPVOID lpParam) {
    // Wait for Steam UI to fully initialize
    Sleep(5000);

    // Get the path to PulseUI.exe (same directory as this DLL)
    wchar_t dllPath[MAX_PATH] = {0};
    HMODULE hSelf = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&PulseThread, &hSelf);
    GetModuleFileNameW(hSelf, dllPath, MAX_PATH);

    // Replace DLL filename with PulseUI.exe
    std::wstring path(dllPath);
    size_t lastSlash = path.find_last_of(L'\\');
    if (lastSlash != std::wstring::npos) {
        path = path.substr(0, lastSlash + 1);
    }
    path += L"PulseUI.exe";

    // Launch PulseUI.exe
    STARTUPINFOW si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // PulseUI manages its own window visibility
    PROCESS_INFORMATION pi = {0};

    CreateProcessW(
        path.c_str(),
        NULL,
        NULL, NULL,
        FALSE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    // Clean up handles
    if (pi.hProcess) CloseHandle(pi.hProcess);
    if (pi.hThread) CloseHandle(pi.hThread);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(NULL, 0, PulseThread, NULL, 0, NULL);
            break;
    }
    return TRUE;
}
