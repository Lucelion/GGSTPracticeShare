#pragma once

#include <windows.h>
#include <string>

namespace GGST {

class Config {
public:
    static void Initialize(HMODULE hModule);
    static void Load();
    static void Save();

    static DWORD GetToggleKey() { return s_ToggleKey; }
    static void SetToggleKey(DWORD vkCode);
    static const std::string& GetToggleKeyName() { return s_ToggleKeyName; }

    static std::string GetKeyDisplayName(DWORD vkCode);
    static const std::wstring& GetModuleDirectory() { return s_ModuleDir; }
    static std::wstring GetConfigFilePath();
    static std::wstring GetLibraryFilePath();
    static std::wstring GetSetLibraryFilePath();

private:
    static DWORD s_ToggleKey;
    static std::string s_ToggleKeyName;
    static std::wstring s_ModuleDir;
};

} // namespace GGST
