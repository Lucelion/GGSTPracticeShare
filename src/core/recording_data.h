#pragma once

#include "../game/ggst_types.h"
#include <string>
#include <vector>

namespace GGST {

struct RecordingItem {
    std::string id;             // 고유 ID (타임스탬프)
    std::string title;          // 녹화 제목 (예: "솔 근S 가드 후 정역 이지 1")
    uint8_t     charId = 0;     // 캐릭터 ID (0 ~ 31)
    std::string charName;       // 캐릭터 이름 (예: "솔 배드가이")
    int32_t     totalFrames = 0;// 총 유효 프레임 수
    int32_t     facingDir = 0;  // 시선 방향 (0 = 오른쪽, 1 = 왼쪽)
    std::string note;           // 권장 위치 / 작성자 메모
    std::string createdAt;      // 생성 일시
    std::vector<uint16_t> inputs;// 프레임별 입력 배열 (최대 2700)

    // 재생 길이(초) 계산
    float GetDurationSeconds() const {
        return totalFrames > 0 ? (float)totalFrames / 60.0f : 0.0f;
    }
};

struct RecordingSetSlot {
    bool enabled = false;        // true면 슬롯에 데이터 적용, false면 적용 시 해당 슬롯 비우기(Clear)
    RecordingItem item;          // 슬롯에 들어갈 녹화 데이터
};

struct RecordingSet {
    std::string id;              // 고유 ID (타임스탬프)
    std::string title;           // 세트 제목 (예: "자토 2K2D 후 샌드위치 정역중하단 세트")
    std::string charName;        // 캐릭터 이름 (예: "자토")
    std::string note;            // 세트 메모 / 조건
    std::string createdAt;       // 생성 일시
    RecordingSetSlot slots[Offsets::MaxSlots]; // 슬롯 1 ~ 5

    int GetActiveSlotCount() const {
        int count = 0;
        for (int i = 0; i < Offsets::MaxSlots; ++i) {
            if (slots[i].enabled && !slots[i].item.inputs.empty()) {
                count++;
            }
        }
        return count;
    }

    std::string GetSlotSummaryText() const {
        std::string s;
        for (int i = 0; i < Offsets::MaxSlots; ++i) {
            if (slots[i].enabled && !slots[i].item.inputs.empty()) {
                if (!s.empty()) s += " / ";
                s += "[" + std::to_string(i + 1) + "] " + slots[i].item.title +
                     "(" + std::to_string(slots[i].item.totalFrames) + "f)";
            }
        }
        return s.empty() ? "모든 슬롯 비어 있음" : s;
    }
};

class RecordingManager {
public:
    // 클립보드 공유 문자열 생성/파싱 (단일 녹화: GGSTREC1:)
    static std::string ExportToShareString(const RecordingItem& item);
    static bool ImportFromShareString(const std::string& shareStr, RecordingItem& outItem, std::string& outError);

    // 클립보드 공유 문자열 생성/파싱 (세트 녹화: GGSTSET1:)
    static bool IsSetShareString(const std::string& shareStr);
    static std::string ExportSetToShareString(const RecordingSet& set);
    static bool ImportSetFromShareString(const std::string& shareStr, RecordingSet& outSet, std::string& outError);

    // 슬롯 <-> RecordingItem 변환
    static RecordingItem SlotToRecordingItem(int slotIndex, const GGSTRecordingSlot& slot,
                                             const std::string& title, const std::string& charName,
                                             const std::string& note);
    static GGSTRecordingSlot RecordingItemToSlot(const RecordingItem& item);

    // 윈도우 시스템 클립보드
    static std::string GetClipboardText();
    static bool SetClipboardText(const std::string& text);

    // 로컬 개별 보관함 (Recordings.json) 파일 입출력
    static bool LoadRecordingLibrary(std::vector<RecordingItem>& outList);
    static bool SaveRecordingLibrary(const std::vector<RecordingItem>& list);

    // 로컬 세트 보관함 (RecordingSets.json) 파일 입출력
    static bool LoadRecordingSetLibrary(std::vector<RecordingSet>& outList);
    static bool SaveRecordingSetLibrary(const std::vector<RecordingSet>& list);
};

} // namespace GGST
