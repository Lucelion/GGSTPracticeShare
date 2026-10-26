#include "config.h"
#include <sstream>
#include <iomanip>

namespace GGST {

DWORD Config::s_ToggleKey = VK_F11;
std::string Config::s_ToggleKeyName = "F11";
std::wstring Config::s_ModuleDir = L".";

void Config::Initialize(HMODULE hModule) {
    wchar_t path[2048] = {0};
    if (GetModuleFileNameW(hModule, path, 2048)) {
        std::wstring ws(path);
        size_t pos = ws.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            // dlls/main.dll 위치일 경우 상위 폴더(GGSTPracticeShare)로 이동
            std::wstring dir = ws.substr(0, pos);
            size_t subPos = dir.find_last_of(L"\\/");
            if (subPos != std::wstring::npos) {
                std::wstring folderName = dir.substr(subPos + 1);
                if (_wcsicmp(folderName.c_str(), L"dlls") == 0) {
                    dir = dir.substr(0, subPos);
                }
            }
            s_ModuleDir = dir;
        }
    }
}

std::wstring Config::GetConfigFilePath() {
    return s_ModuleDir + L"\\config.ini";
}

std::wstring Config::GetLibraryFilePath() {
    return s_ModuleDir + L"\\Recordings.json";
}

std::wstring Config::GetSetLibraryFilePath() {
    return s_ModuleDir + L"\\RecordingSets.json";
}

std::string Config::GetKeyDisplayName(DWORD vkCode) {
    if (vkCode >= VK_F1 && vkCode <= VK_F24) {
        return "F" + std::to_string(vkCode - VK_F1 + 1);
    }
    switch (vkCode) {
        case VK_ESCAPE:   return "ESC";
        case VK_TAB:      return "TAB";
        case VK_RETURN:   return "ENTER";
        case VK_SPACE:    return "SPACE";
        case VK_INSERT:   return "INSERT";
        case VK_DELETE:   return "DELETE";
        case VK_HOME:     return "HOME";
        case VK_END:      return "END";
        case VK_PRIOR:    return "PAGE UP";
        case VK_NEXT:     return "PAGE DOWN";
        case VK_PAUSE:    return "PAUSE";
        case VK_SNAPSHOT: return "PRINT SCREEN";
        case VK_SCROLL:   return "SCROLL LOCK";
        case VK_OEM_3:    return "` (Tilde)";
        case VK_OEM_MINUS:return "-";
        case VK_OEM_PLUS: return "=";
        case VK_OEM_4:    return "[";
        case VK_OEM_6:    return "]";
        case VK_OEM_5:    return "\\";
        case VK_OEM_1:    return ";";
        case VK_OEM_7:    return "'";
        case VK_OEM_COMMA: return ",";
        case VK_OEM_PERIOD:return ".";
        case VK_OEM_2:    return "/";
        default: break;
    }

    if ((vkCode >= 'A' && vkCode <= 'Z') || (vkCode >= '0' && vkCode <= '9')) {
        return std::string(1, (char)vkCode);
    }

    if (vkCode >= VK_NUMPAD0 && vkCode <= VK_NUMPAD9) {
        return "NUM " + std::to_string(vkCode - VK_NUMPAD0);
    }

    char hexBuf[16];
    snprintf(hexBuf, sizeof(hexBuf), "0x%02X", (unsigned int)vkCode);
    return std::string(hexBuf);
}

void Config::SetToggleKey(DWORD vkCode) {
    s_ToggleKey = vkCode;
    s_ToggleKeyName = GetKeyDisplayName(vkCode);
    Save();
}

void Config::Load() {
    std::wstring iniPath = GetConfigFilePath();

    UINT key = GetPrivateProfileIntW(L"Keybind", L"ToggleKey", VK_F11, iniPath.c_str());
    s_ToggleKey = key;

    wchar_t nameBuf[64] = {0};
    GetPrivateProfileStringW(L"Keybind", L"ToggleKeyName", L"F11", nameBuf, 64, iniPath.c_str());

    char mbName[64] = {0};
    WideCharToMultiByte(CP_UTF8, 0, nameBuf, -1, mbName, sizeof(mbName), NULL, NULL);
    s_ToggleKeyName = mbName;

    if (s_ToggleKeyName.empty()) {
        s_ToggleKeyName = GetKeyDisplayName(s_ToggleKey);
    }
}

void Config::Save() {
    std::wstring iniPath = GetConfigFilePath();

    std::wstring keyStr = std::to_wstring(s_ToggleKey);
    WritePrivateProfileStringW(L"Keybind", L"ToggleKey", keyStr.c_str(), iniPath.c_str());

    wchar_t wNameBuf[64] = {0};
    MultiByteToWideChar(CP_UTF8, 0, s_ToggleKeyName.c_str(), -1, wNameBuf, 64);
    WritePrivateProfileStringW(L"Keybind", L"ToggleKeyName", wNameBuf, iniPath.c_str());
}

} // namespace GGST
