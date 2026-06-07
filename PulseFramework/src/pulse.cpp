#include <windows.h>
#include <string>

std::wstring GetRegistryString(HKEY hKey, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hTestKey;
    if (RegOpenKeyExW(hKey, subKey.c_str(), 0, KEY_READ, &hTestKey) == ERROR_SUCCESS) {
        DWORD type = REG_SZ;
        DWORD cbData = 0;
        if (RegQueryValueExW(hTestKey, valueName.c_str(), NULL, &type, NULL, &cbData) == ERROR_SUCCESS && type == REG_SZ) {
            std::wstring value(cbData / sizeof(wchar_t), L'\0');
            if (RegQueryValueExW(hTestKey, valueName.c_str(), NULL, NULL, reinterpret_cast<LPBYTE>(&value[0]), &cbData) == ERROR_SUCCESS) {
                // Remove trailing null terminators
                while (!value.empty() && value.back() == L'\0') {
                    value.pop_back();
                }
                RegCloseKey(hTestKey);
                return value;
            }
        }
        RegCloseKey(hTestKey);
    }
    return L"";
}

void ClearRegistryString(HKEY hKey, const std::wstring& subKey, const std::wstring& valueName) {
    HKEY hTestKey;
    if (RegOpenKeyExW(hKey, subKey.c_str(), 0, KEY_SET_VALUE, &hTestKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hTestKey, valueName.c_str());
        RegCloseKey(hTestKey);
    }
}

DWORD WINAPI PulseThread(LPVOID lpParam) {
    // Wait a few seconds for Steam UI to initialize
    Sleep(5000);

    std::wstring subKey = L"Software\\leVi Studios\\LuaPatcher";
    std::wstring valueName = L"PulseFramework/LastAdded";
    
    std::wstring lastAdded = GetRegistryString(HKEY_CURRENT_USER, subKey, valueName);
    
    std::wstring message = L"👋 Welcome back!\n\nSteam Library Manager is ready to help you organize your collection, discover your installed games, and keep everything exactly where you want it.\n\nHappy gaming!";
    
    if (!lastAdded.empty()) {
        message += L"\n\nP.S. You recently added: " + lastAdded + L"!";
        // Clear it so it only shows once
        ClearRegistryString(HKEY_CURRENT_USER, subKey, valueName);
    }

    MessageBoxW(NULL, message.c_str(), L"Pulse Framework", MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TOPMOST);
    
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
