#pragma once

#include "../core/recording_data.h"
#include "../vendor/imgui/imgui.h"
#include <string>
#include <vector>

namespace GGST {

struct HangulEditOp {
    bool replacePrevChar = false;
    bool deletePrevCharOnly = false;
    wchar_t newChar = 0;
};

struct HangulAutomataState {
    int cho = -1;
    int jung = -1;
    int jung1 = -1;
    int jung2 = -1;
    int jong = 0;
    int jong1 = 0;
    int jong1_cho = -1;
    int jong2_cho = -1;

    bool IsComposing() const { return cho != -1 || jung != -1; }
    void Reset() {
        cho = -1;
        jung = -1;
        jung1 = -1;
        jung2 = -1;
        jong = 0;
        jong1 = 0;
        jong1_cho = -1;
        jong2_cho = -1;
    }
    wchar_t GetCurrentChar() const;
};

class UI {
public:
    static void Initialize();
    static void Render();

    // 상태 알림 메시지
    static void ShowToast(const std::string& msg, float durationSec = 3.0f);

    // 내장 한글 오토마타(두벌식) 제어 인터페이스
    static bool IsKoreanMode() { return s_KoreanMode; }
    static void ToggleKoreanMode();
    static bool IsHangulComposing() { return s_HangulState.IsComposing(); }
    static void ResetHangulState();
    static void OnHangulChar(char asciiChar);
    static void OnHangulBackspace();

private:
    static void RenderHeader();
    static void RenderSlots();
    static void RenderConfirmModal();
    static void RenderConfirmSetModal();
    static void RenderBottomSection();
    static void RenderLibrary();
    static void RenderSetLibrary();
    static void RenderExportModal();
    static void RenderSetModal();
    static void SetupTheme();
    static void SortLibrary(int columnIdx, bool ascending);
    static void SortSetLibrary(int columnIdx, bool ascending);
    static bool ApplySetToGameSlots(const RecordingSet& set);
    static void OpenSetModalFromCurrentGameSlots();

    // 한글 입력이 적용된 ImGui InputText 래퍼
    static bool InputTextKorean(const char* label, char* buf, size_t bufSize, ImGuiInputTextFlags flags = 0);
    static bool InputTextWithHintKorean(const char* label, const char* hint, char* buf, size_t bufSize, ImGuiInputTextFlags flags = 0);
    static int HangulInputTextCallback(ImGuiInputTextCallbackData* data);

    static bool s_ShowLibrary;
    static int  s_ActiveTab;           // 0: 개별 녹화 보관함, 1: 세트 보관함
    static int  s_RequestedTab;        // -1: 변경 없음, 0: 개별 탭 강제 선택, 1: 세트 탭 강제 선택
    static bool s_ShowConfirmModal;
    static bool s_ShowConfirmSetModal;
    static bool s_ShowExportModal;
    static bool s_ShowSetModal;

    static RecordingItem s_PendingConfirmItem;
    static int s_ConfirmTargetSlot;
    static RecordingSet s_PendingConfirmSet;

    // 슬롯 내보내기 / 보관함 항목 수정용 상태
    static int s_ExportSourceSlot;     // 0~4: 슬롯에서 내보내기, -1: 보관함 기존 항목 수정
    static int s_EditingLibraryIndex;  // 보관함 수정 중인 인덱스 (-1이면 신규)
    static char s_ExportTitleBuf[256];
    static char s_ExportCharBuf[128];
    static char s_ExportNoteBuf[512];

    // 세트 생성 / 수정 모달용 상태
    static int s_EditingSetIndex;      // 세트 보관함 수정 중인 인덱스 (-1이면 신규)
    static RecordingSet s_EditingSetDraft;
    static char s_SetTitleBuf[256];
    static char s_SetCharBuf[128];
    static char s_SetNoteBuf[512];
    static char s_SetSlotTitleBufs[Offsets::MaxSlots][128];

    // 보관함 검색 필터
    static char s_SearchFilterBuf[128];
    static char s_SetSearchFilterBuf[128];

    // 로컬 보관함 캐시
    static std::vector<RecordingItem> s_Library;
    static std::vector<int> s_RowTargetSlots;
    static std::vector<RecordingSet> s_SetLibrary;

    // 토스트 알림
    static std::string s_ToastMessage;
    static float s_ToastTimer;

    // 내장 두벌식 한글 오토마타 상태
    static bool s_KoreanMode;
    static HangulAutomataState s_HangulState;
    static std::vector<HangulEditOp> s_PendingHangulOps;
    static ImGuiID s_LastActiveInputId;
};

} // namespace GGST
