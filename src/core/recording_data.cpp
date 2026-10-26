#include "recording_data.h"
#include "config.h"
#include "../vendor/miniz/miniz.h"
#include "../vendor/nlohmann/json.hpp"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

using json = nlohmann::json;

namespace GGST {

// --- Base64 유틸리티 ---
static const std::string BASE64_CHARS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static std::string Base64Encode(const unsigned char* bytes_to_encode, size_t in_len) {
    std::string ret;
    int i = 0, j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                ret += BASE64_CHARS[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < (i + 1); j++)
            ret += BASE64_CHARS[char_array_4[j]];

        while ((i++ < 3))
            ret += '=';
    }

    return ret;
}

static std::vector<unsigned char> Base64Decode(const std::string& encoded_string) {
    size_t in_len = encoded_string.size();
    int i = 0, j = 0, in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (encoded_string[in_] != '=') &&
           (isalnum((unsigned char)encoded_string[in_]) || (encoded_string[in_] == '+') || (encoded_string[in_] == '/'))) {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = (unsigned char)BASE64_CHARS.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = (((char_array_4[2] & 0x3) << 6) + char_array_4[3]);

            for (i = 0; i < 3; i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = 0; j < i; j++)
            char_array_4[j] = (unsigned char)BASE64_CHARS.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; j < i - 1; j++)
            ret.push_back(char_array_3[j]);
    }

    return ret;
}

// --- RecordingManager 구현 ---

std::string RecordingManager::ExportToShareString(const RecordingItem& item) {
    json j;
    j["v"] = 1; // 버전
    j["t"] = item.title;
    j["c"] = item.charId;
    j["cn"] = item.charName;
    j["f"] = item.totalFrames;
    j["d"] = item.facingDir;
    j["n"] = item.note;
    j["i"] = item.inputs;

    std::string jsonStr = j.dump();

    // miniz(zlib) 압축
    uLong uncompLen = (uLong)jsonStr.size();
    uLong compLen = compressBound(uncompLen);
    std::vector<unsigned char> compBuf(compLen);

    if (compress(compBuf.data(), &compLen, (const unsigned char*)jsonStr.data(), uncompLen) != Z_OK) {
        return "";
    }

    // Base64 인코딩
    std::string b64 = Base64Encode(compBuf.data(), compLen);
    return "GGSTREC1:" + b64;
}

bool RecordingManager::ImportFromShareString(const std::string& shareStr, RecordingItem& outItem, std::string& outError) {
    std::string trimmed = shareStr;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    const std::string prefix = "GGSTREC1:";
    if (trimmed.rfind(prefix, 0) != 0) {
        outError = "올바른 GGST 녹화 코드 포맷이 아닙니다 (GGSTREC1: 또는 GGSTSET1: 접두사 필요).";
        return false;
    }

    std::string b64 = trimmed.substr(prefix.length());
    // 공백, 줄바꿈 제거
    b64.erase(std::remove_if(b64.begin(), b64.end(), [](char c) { return isspace((unsigned char)c); }), b64.end());

    std::vector<unsigned char> compData = Base64Decode(b64);
    if (compData.empty()) {
        outError = "Base64 디코딩에 실패했습니다.";
        return false;
    }

    // miniz 압축 해제 (최대 1MB 버퍼 가정)
    uLong uncompLen = 1024 * 1024;
    std::vector<unsigned char> uncompBuf(uncompLen);

    int zRes = uncompress(uncompBuf.data(), &uncompLen, compData.data(), (uLong)compData.size());
    if (zRes != Z_OK) {
        outError = "압축 해제(zlib) 실패 (손상된 데이터).";
        return false;
    }

    std::string jsonStr(reinterpret_cast<char*>(uncompBuf.data()), uncompLen);

    try {
        json j = json::parse(jsonStr);
        outItem.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        outItem.title = j.value("t", "이름 없는 녹화");
        outItem.charId = j.value("c", (uint8_t)0);
        outItem.charName = j.value("cn", std::string("미지정"));
        if (outItem.charName.empty()) {
            outItem.charName = "미지정";
        }
        outItem.totalFrames = j.value("f", 0);
        outItem.facingDir = j.value("d", 0);
        outItem.note = j.value("n", "");

        if (j.contains("i") && j["i"].is_array()) {
            outItem.inputs = j["i"].get<std::vector<uint16_t>>();
        } else {
            outError = "입력 데이터 배열이 누락되었습니다.";
            return false;
        }

        if (outItem.totalFrames <= 0 || outItem.inputs.empty()) {
            outError = "유효한 녹화 프레임이 없습니다.";
            return false;
        }

        if (outItem.totalFrames > Offsets::MaxFrames || (int32_t)outItem.inputs.size() > Offsets::MaxFrames) {
            outError = "녹화 프레임 길이가 최대 한도(2700프레임 / 45초)를 초과합니다.";
            return false;
        }
        outItem.totalFrames = (int32_t)outItem.inputs.size();

        // 현재 시간 기록
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tmNow;
        localtime_s(&tmNow, &now);
        char timeBuf[64];
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", &tmNow);
        outItem.createdAt = timeBuf;

        return true;
    } catch (const std::exception& e) {
        outError = std::string("JSON 파싱 에러: ") + e.what();
        return false;
    }
}

RecordingItem RecordingManager::SlotToRecordingItem(int slotIndex, const GGSTRecordingSlot& slot,
                                                    const std::string& title, const std::string& charName,
                                                    const std::string& note) {
    RecordingItem item;
    item.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    item.title = title.empty() ? ("슬롯 " + std::to_string(slotIndex + 1) + " 녹화") : title;
    item.charId = 0;
    item.charName = charName.empty() ? "미지정" : charName;
    item.facingDir = slot.RecordedFacingDir;
    item.note = note;

    int32_t duration = slot.TotalFrameCount - slot.StartOffsetFrame;
    if (duration <= 0 || duration > Offsets::MaxFrames) {
        duration = slot.TotalFrameCount;
    }
    if (duration < 0) duration = 0;
    if (duration > Offsets::MaxFrames) duration = Offsets::MaxFrames;
    item.totalFrames = duration;

    item.inputs.resize(duration);
    int32_t startOffset = slot.StartOffsetFrame >= 0 ? slot.StartOffsetFrame : 0;
    for (int32_t i = 0; i < duration; ++i) {
        int32_t srcIdx = startOffset + i;
        if (srcIdx < Offsets::MaxFrames) {
            item.inputs[i] = slot.FrameInputs[srcIdx];
        } else {
            item.inputs[i] = 0;
        }
    }

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tmNow;
    localtime_s(&tmNow, &now);
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", &tmNow);
    item.createdAt = timeBuf;

    return item;
}

GGSTRecordingSlot RecordingManager::RecordingItemToSlot(const RecordingItem& item) {
    GGSTRecordingSlot slot{};
    slot.bHasData = 1;
    slot.RecordedFacingDir = item.facingDir;
    slot.StartOffsetFrame = 0;

    int32_t count = (int32_t)item.inputs.size();
    if (count > Offsets::MaxFrames) count = Offsets::MaxFrames;

    // GGST 엔진(RVA 0xBCBE90)은 TotalFrameCount <= 4 일 때 빈 슬롯으로 판정하므로 최소 5프레임 보장
    slot.TotalFrameCount = (count > 0 && count < 5) ? 5 : count;

    for (int32_t i = 0; i < count; ++i) {
        slot.FrameInputs[i] = item.inputs[i];
    }
    // 남은 프레임은 0으로 채움
    for (int32_t i = count; i < Offsets::MaxFrames; ++i) {
        slot.FrameInputs[i] = 0;
    }

    return slot;
}

std::string RecordingManager::GetClipboardText() {
    if (!OpenClipboard(NULL)) return "";

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return "";
    }

    wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
    if (!pszText) {
        CloseClipboard();
        return "";
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, pszText, -1, NULL, 0, NULL, NULL);
    std::string str(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, pszText, -1, &str[0], size_needed, NULL, NULL);

    GlobalUnlock(hData);
    CloseClipboard();
    return str;
}

bool RecordingManager::SetClipboardText(const std::string& text) {
    if (!OpenClipboard(NULL)) return false;
    EmptyClipboard();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (!hGlob) {
        CloseClipboard();
        return false;
    }

    wchar_t* pBuf = static_cast<wchar_t*>(GlobalLock(hGlob));
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pBuf, wlen);
    GlobalUnlock(hGlob);

    SetClipboardData(CF_UNICODETEXT, hGlob);
    CloseClipboard();
    return true;
}

bool RecordingManager::LoadRecordingLibrary(std::vector<RecordingItem>& outList) {
    outList.clear();
    std::wstring libPath = Config::GetLibraryFilePath();

    std::ifstream inFile(libPath);
    if (!inFile.is_open()) {
        return false;
    }

    try {
        json j;
        inFile >> j;
        if (!j.is_array()) return false;

        for (const auto& itemJson : j) {
            RecordingItem item;
            item.id = itemJson.value("id", "");
            item.title = itemJson.value("title", "");
            item.charId = itemJson.value("charId", (uint8_t)0);
            item.charName = itemJson.value("charName", "");
            item.totalFrames = itemJson.value("totalFrames", 0);
            item.facingDir = itemJson.value("facingDir", 0);
            item.note = itemJson.value("note", "");
            item.createdAt = itemJson.value("createdAt", "");

            if (itemJson.contains("inputs") && itemJson["inputs"].is_array()) {
                item.inputs = itemJson["inputs"].get<std::vector<uint16_t>>();
            }
            outList.push_back(item);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool RecordingManager::SaveRecordingLibrary(const std::vector<RecordingItem>& list) {
    std::wstring libPath = Config::GetLibraryFilePath();

    json j = json::array();
    for (const auto& item : list) {
        json itemJson;
        itemJson["id"] = item.id;
        itemJson["title"] = item.title;
        itemJson["charId"] = item.charId;
        itemJson["charName"] = item.charName;
        itemJson["totalFrames"] = item.totalFrames;
        itemJson["facingDir"] = item.facingDir;
        itemJson["note"] = item.note;
        itemJson["createdAt"] = item.createdAt;
        itemJson["inputs"] = item.inputs;
        j.push_back(itemJson);
    }

    std::ofstream outFile(libPath);
    if (!outFile.is_open()) {
        return false;
    }

    outFile << j.dump(2);
    return true;
}

bool RecordingManager::IsSetShareString(const std::string& shareStr) {
    std::string trimmed = shareStr;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
        trimmed.erase(0, first);
    }
    return trimmed.rfind("GGSTSET1:", 0) == 0;
}

std::string RecordingManager::ExportSetToShareString(const RecordingSet& set) {
    json j;
    j["v"] = 1;
    j["t"] = set.title;
    j["cn"] = set.charName;
    j["n"] = set.note;

    json slotsArr = json::array();
    for (int s = 0; s < Offsets::MaxSlots; ++s) {
        json sj;
        bool en = set.slots[s].enabled && !set.slots[s].item.inputs.empty();
        sj["en"] = en;
        if (en) {
            sj["t"] = set.slots[s].item.title;
            sj["cn"] = set.slots[s].item.charName;
            sj["f"] = (int32_t)set.slots[s].item.inputs.size();
            sj["d"] = set.slots[s].item.facingDir;
            sj["n"] = set.slots[s].item.note;
            sj["i"] = set.slots[s].item.inputs;
        }
        slotsArr.push_back(sj);
    }
    j["slots"] = slotsArr;

    std::string jsonStr = j.dump();

    uLong uncompLen = (uLong)jsonStr.size();
    uLong compLen = compressBound(uncompLen);
    std::vector<unsigned char> compBuf(compLen);

    if (compress(compBuf.data(), &compLen, (const unsigned char*)jsonStr.data(), uncompLen) != Z_OK) {
        return "";
    }

    std::string b64 = Base64Encode(compBuf.data(), compLen);
    return "GGSTSET1:" + b64;
}

bool RecordingManager::ImportSetFromShareString(const std::string& shareStr, RecordingSet& outSet, std::string& outError) {
    std::string trimmed = shareStr;
    size_t first = trimmed.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
        trimmed.erase(0, first);
    }

    const std::string prefix = "GGSTSET1:";
    if (trimmed.rfind(prefix, 0) != 0) {
        outError = "올바른 GGST 세트 코드 포맷이 아닙니다 (GGSTSET1: 접두사 필요).";
        return false;
    }

    std::string b64 = trimmed.substr(prefix.length());
    b64.erase(std::remove_if(b64.begin(), b64.end(), [](char c) { return isspace((unsigned char)c); }), b64.end());

    std::vector<unsigned char> compData = Base64Decode(b64);
    if (compData.empty()) {
        outError = "Base64 디코딩에 실패했습니다.";
        return false;
    }

    // 세트 데이터(최대 5슬롯) 압축 해제를 위해 4MB 버퍼 할당
    uLong uncompLen = 4 * 1024 * 1024;
    std::vector<unsigned char> uncompBuf(uncompLen);

    int zRes = uncompress(uncompBuf.data(), &uncompLen, compData.data(), (uLong)compData.size());
    if (zRes != Z_OK) {
        outError = "압축 해제(zlib) 실패 (손상된 세트 데이터).";
        return false;
    }

    std::string jsonStr(reinterpret_cast<char*>(uncompBuf.data()), uncompLen);

    try {
        json j = json::parse(jsonStr);
        outSet = RecordingSet{};
        outSet.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        outSet.title = j.value("t", "이름 없는 세트");
        outSet.charName = j.value("cn", "미지정");
        if (outSet.charName.empty()) outSet.charName = "미지정";
        outSet.note = j.value("n", "");

        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tmNow;
        localtime_s(&tmNow, &now);
        char timeBuf[64];
        std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", &tmNow);
        outSet.createdAt = timeBuf;

        if (!j.contains("slots") || !j["slots"].is_array()) {
            outError = "슬롯 배열 정보가 누락되었습니다.";
            return false;
        }

        const auto& slotsArr = j["slots"];
        int activeCount = 0;
        for (int s = 0; s < Offsets::MaxSlots && s < (int)slotsArr.size(); ++s) {
            const auto& sj = slotsArr[s];
            bool en = sj.value("en", false);
            outSet.slots[s].enabled = en;
            if (en && sj.contains("i") && sj["i"].is_array()) {
                auto& item = outSet.slots[s].item;
                item.id = outSet.id + "_" + std::to_string(s + 1);
                item.title = sj.value("t", "슬롯 " + std::to_string(s + 1));
                item.charName = sj.value("cn", outSet.charName);
                item.facingDir = sj.value("d", 0);
                item.note = sj.value("n", "");
                item.inputs = sj["i"].get<std::vector<uint16_t>>();
                if ((int32_t)item.inputs.size() > Offsets::MaxFrames) {
                    item.inputs.resize(Offsets::MaxFrames);
                }
                item.totalFrames = (int32_t)item.inputs.size();
                item.createdAt = outSet.createdAt;
                if (!item.inputs.empty()) {
                    activeCount++;
                } else {
                    outSet.slots[s].enabled = false;
                }
            } else {
                outSet.slots[s].enabled = false;
            }
        }

        if (activeCount == 0) {
            outError = "세트에 유효한 녹화 슬롯 데이터가 하나도 없습니다.";
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        outError = std::string("세트 JSON 파싱 에러: ") + e.what();
        return false;
    }
}

bool RecordingManager::LoadRecordingSetLibrary(std::vector<RecordingSet>& outList) {
    outList.clear();
    std::wstring libPath = Config::GetSetLibraryFilePath();

    std::ifstream inFile(libPath);
    if (!inFile.is_open()) {
        return false;
    }

    try {
        json j;
        inFile >> j;
        if (!j.is_array()) return false;

        for (const auto& setJson : j) {
            RecordingSet set;
            set.id = setJson.value("id", "");
            set.title = setJson.value("title", "");
            set.charName = setJson.value("charName", "미지정");
            set.note = setJson.value("note", "");
            set.createdAt = setJson.value("createdAt", "");

            if (setJson.contains("slots") && setJson["slots"].is_array()) {
                const auto& slotsArr = setJson["slots"];
                for (int s = 0; s < Offsets::MaxSlots && s < (int)slotsArr.size(); ++s) {
                    const auto& sj = slotsArr[s];
                    set.slots[s].enabled = sj.value("enabled", false);
                    if (set.slots[s].enabled && sj.contains("inputs") && sj["inputs"].is_array()) {
                        auto& item = set.slots[s].item;
                        item.id = sj.value("id", "");
                        item.title = sj.value("title", "슬롯 " + std::to_string(s + 1));
                        item.charName = sj.value("charName", set.charName);
                        item.facingDir = sj.value("facingDir", 0);
                        item.note = sj.value("note", "");
                        item.createdAt = sj.value("createdAt", set.createdAt);
                        item.inputs = sj["inputs"].get<std::vector<uint16_t>>();
                        item.totalFrames = (int32_t)item.inputs.size();
                        if (item.inputs.empty()) {
                            set.slots[s].enabled = false;
                        }
                    } else {
                        set.slots[s].enabled = false;
                    }
                }
            }
            outList.push_back(set);
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool RecordingManager::SaveRecordingSetLibrary(const std::vector<RecordingSet>& list) {
    std::wstring libPath = Config::GetSetLibraryFilePath();

    json j = json::array();
    for (const auto& set : list) {
        json setJson;
        setJson["id"] = set.id;
        setJson["title"] = set.title;
        setJson["charName"] = set.charName;
        setJson["note"] = set.note;
        setJson["createdAt"] = set.createdAt;

        json slotsArr = json::array();
        for (int s = 0; s < Offsets::MaxSlots; ++s) {
            json sj;
            bool en = set.slots[s].enabled && !set.slots[s].item.inputs.empty();
            sj["enabled"] = en;
            if (en) {
                sj["id"] = set.slots[s].item.id;
                sj["title"] = set.slots[s].item.title;
                sj["charName"] = set.slots[s].item.charName;
                sj["totalFrames"] = set.slots[s].item.totalFrames;
                sj["facingDir"] = set.slots[s].item.facingDir;
                sj["note"] = set.slots[s].item.note;
                sj["createdAt"] = set.slots[s].item.createdAt;
                sj["inputs"] = set.slots[s].item.inputs;
            }
            slotsArr.push_back(sj);
        }
        setJson["slots"] = slotsArr;
        j.push_back(setJson);
    }

    std::ofstream outFile(libPath);
    if (!outFile.is_open()) {
        return false;
    }

    outFile << j.dump(2);
    return true;
}

} // namespace GGST
