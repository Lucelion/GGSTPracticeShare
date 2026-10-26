#include "ui.h"
#include "../game/game_memory.h"
#include "../core/config.h"
#include "../core/dx11_hook.h"
#include "../vendor/imgui/imgui.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstring>

namespace GGST {

bool UI::s_ShowLibrary = true; // 기본으로 로컬 보관함도 함께 표시
int  UI::s_ActiveTab = 0;      // 0: 개별 녹화 보관함, 1: 세트 보관함
int  UI::s_RequestedTab = -1;
bool UI::s_ShowConfirmModal = false;
bool UI::s_ShowConfirmSetModal = false;
bool UI::s_ShowExportModal = false;
bool UI::s_ShowSetModal = false;

RecordingItem UI::s_PendingConfirmItem;
int UI::s_ConfirmTargetSlot = 0;
RecordingSet UI::s_PendingConfirmSet;

int UI::s_ExportSourceSlot = 0;
int UI::s_EditingLibraryIndex = -1;
char UI::s_ExportTitleBuf[256] = {0};
char UI::s_ExportCharBuf[128] = {0};
char UI::s_ExportNoteBuf[512] = {0};

int UI::s_EditingSetIndex = -1;
RecordingSet UI::s_EditingSetDraft;
char UI::s_SetTitleBuf[256] = {0};
char UI::s_SetCharBuf[128] = {0};
char UI::s_SetNoteBuf[512] = {0};
char UI::s_SetSlotTitleBufs[Offsets::MaxSlots][128] = {{0}};

char UI::s_SearchFilterBuf[128] = {0};
char UI::s_SetSearchFilterBuf[128] = {0};

std::vector<RecordingItem> UI::s_Library;
std::vector<int> UI::s_RowTargetSlots;
std::vector<RecordingSet> UI::s_SetLibrary;
std::string UI::s_ToastMessage = "";
float UI::s_ToastTimer = 0.0f;

// 기본 한글 입력 모드 활성 (한/영 키 또는 UI 버튼으로 언제든 영문 전환 가능)
bool UI::s_KoreanMode = true;
HangulAutomataState UI::s_HangulState;
std::vector<HangulEditOp> UI::s_PendingHangulOps;
ImGuiID UI::s_LastActiveInputId = 0;

// =====================================================================
// 내장 두벌식 한글 오토마타 구현 (UE4의 IME 차단 환경을 완벽 우회)
// =====================================================================
namespace {

static const wchar_t kCompatChoseong[19] = {
    0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145,
    0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E
};

static const wchar_t kCompatJungseong[21] = {
    0x314F, 0x3150, 0x3151, 0x3152, 0x3153, 0x3154, 0x3155, 0x3156, 0x3157, 0x3158,
    0x3159, 0x315A, 0x315B, 0x315C, 0x315D, 0x315E, 0x315F, 0x3160, 0x3161, 0x3162, 0x3163
};

static int AsciiToChoseong(char c) {
    switch (c) {
    case 'r': return 0;  // ㄱ
    case 'R': return 1;  // ㄲ
    case 's': case 'S': return 2;  // ㄴ
    case 'e': return 3;  // ㄷ
    case 'E': return 4;  // ㄸ
    case 'f': case 'F': return 5;  // ㄹ
    case 'a': case 'A': return 6;  // ㅁ
    case 'q': return 7;  // ㅂ
    case 'Q': return 8;  // ㅃ
    case 't': return 9;  // ㅅ
    case 'T': return 10; // ㅆ
    case 'd': case 'D': return 11; // ㅇ
    case 'w': return 12; // ㅈ
    case 'W': return 13; // ㅉ
    case 'c': case 'C': return 14; // ㅊ
    case 'z': case 'Z': return 15; // ㅋ
    case 'x': case 'X': return 16; // ㅌ
    case 'v': case 'V': return 17; // ㅍ
    case 'g': case 'G': return 18; // ㅎ
    default: return -1;
    }
}

static int AsciiToJungseong(char c) {
    switch (c) {
    case 'k': case 'K': return 0;  // ㅏ
    case 'o': return 1;            // ㅐ
    case 'i': case 'I': return 2;  // ㅑ
    case 'O': return 3;            // ㅒ
    case 'j': case 'J': return 4;  // ㅓ
    case 'p': return 5;            // ㅔ
    case 'u': case 'U': return 6;  // ㅕ
    case 'P': return 7;            // ㅖ
    case 'h': case 'H': return 8;  // ㅗ
    case 'y': case 'Y': return 12; // ㅛ
    case 'n': case 'N': return 13; // ㅜ
    case 'b': case 'B': return 17; // ㅠ
    case 'm': case 'M': return 18; // ㅡ
    case 'l': case 'L': return 20; // ㅣ
    default: return -1;
    }
}

static int CombineJungseong(int first, int second) {
    if (first == 8) { // ㅗ
        if (second == 0) return 9;   // ㅘ
        if (second == 1) return 10;  // ㅙ
        if (second == 20) return 11; // ㅚ
    } else if (first == 13) { // ㅜ
        if (second == 4) return 14;  // ㅝ
        if (second == 5) return 15;  // ㅞ
        if (second == 20) return 16; // ㅟ
    } else if (first == 18) { // ㅡ
        if (second == 20) return 19; // ㅢ
    }
    return -1;
}

static int ChoseongToJongseong(int cho) {
    switch (cho) {
    case 0: return 1;   // ㄱ
    case 1: return 2;   // ㄲ
    case 2: return 4;   // ㄴ
    case 3: return 7;   // ㄷ
    case 5: return 8;   // ㄹ
    case 6: return 16;  // ㅁ
    case 7: return 17;  // ㅂ
    case 9: return 19;  // ㅅ
    case 10: return 20; // ㅆ
    case 11: return 21; // ㅇ
    case 12: return 22; // ㅈ
    case 14: return 23; // ㅊ
    case 15: return 24; // ㅋ
    case 16: return 25; // ㅌ
    case 17: return 26; // ㅍ
    case 18: return 27; // ㅎ
    default: return 0;  // ㄸ, ㅃ, ㅉ는 받침 불가
    }
}

static int CombineJongseong(int firstJong, int secondCho) {
    if (firstJong == 1 && secondCho == 9) return 3;   // ㄳ
    if (firstJong == 4 && secondCho == 12) return 5;  // ㄵ
    if (firstJong == 4 && secondCho == 18) return 6;  // ㄶ
    if (firstJong == 8 && secondCho == 0) return 9;   // ㄺ
    if (firstJong == 8 && secondCho == 6) return 10;  // ㄻ
    if (firstJong == 8 && secondCho == 7) return 11;  // ㄼ
    if (firstJong == 8 && secondCho == 9) return 12;  // ㄽ
    if (firstJong == 8 && secondCho == 16) return 13; // ㄾ
    if (firstJong == 8 && secondCho == 17) return 14; // ㄿ
    if (firstJong == 8 && secondCho == 18) return 15; // ㅀ
    if (firstJong == 17 && secondCho == 9) return 18; // ㅄ
    return 0;
}

} // anonymous namespace

wchar_t HangulAutomataState::GetCurrentChar() const {
    if (cho != -1 && jung != -1) {
        return (wchar_t)(0xAC00 + (cho * 21 + jung) * 28 + jong);
    }
    if (cho != -1) {
        return kCompatChoseong[cho];
    }
    if (jung != -1) {
        return kCompatJungseong[jung];
    }
    return 0;
}

void UI::ToggleKoreanMode() {
    s_HangulState.Reset();
    s_KoreanMode = !s_KoreanMode;
}

void UI::ResetHangulState() {
    s_HangulState.Reset();
}

void UI::OnHangulChar(char asciiChar) {
    int cho = AsciiToChoseong(asciiChar);
    int jung = AsciiToJungseong(asciiChar);

    if (cho != -1) {
        // 자음 입력
        if (!s_HangulState.IsComposing()) {
            s_HangulState.cho = cho;
            s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
        } else if (s_HangulState.cho != -1 && s_HangulState.jung == -1) {
            // 초성만 있는 상태에서 또 자음이 오면 이전 초성 확정 후 새 초성 시작
            s_HangulState.Reset();
            s_HangulState.cho = cho;
            s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
        } else if (s_HangulState.cho == -1 && s_HangulState.jung != -1) {
            // 모음만 있는 상태에서 자음이 오면 새 초성 시작
            s_HangulState.Reset();
            s_HangulState.cho = cho;
            s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
        } else if (s_HangulState.jong == 0) {
            // 초성+중성 상태에서 첫 받침 시도
            int newJong = ChoseongToJongseong(cho);
            if (newJong != 0) {
                s_HangulState.jong = newJong;
                s_HangulState.jong1 = newJong;
                s_HangulState.jong1_cho = cho;
                s_HangulState.jong2_cho = -1;
                s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
            } else {
                // ㄸ, ㅃ, ㅉ 등 받침 불가 자음은 새 음절 초성으로 시작
                s_HangulState.Reset();
                s_HangulState.cho = cho;
                s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
            }
        } else if (s_HangulState.jong2_cho == -1) {
            // 단일 받침 상태에서 겹받침 결합 시도
            int combinedJong = CombineJongseong(s_HangulState.jong1, cho);
            if (combinedJong != 0) {
                s_HangulState.jong = combinedJong;
                s_HangulState.jong2_cho = cho;
                s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
            } else {
                s_HangulState.Reset();
                s_HangulState.cho = cho;
                s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
            }
        } else {
            // 이미 겹받침이 꽉 찬 상태면 새 음절 초성으로 시작
            s_HangulState.Reset();
            s_HangulState.cho = cho;
            s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
        }
    } else if (jung != -1) {
        // 모음 입력
        if (!s_HangulState.IsComposing()) {
            s_HangulState.jung = jung;
            s_HangulState.jung1 = jung;
            s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
        } else if (s_HangulState.cho != -1 && s_HangulState.jung == -1) {
            // 초성만 있는 상태에서 중성 결합
            s_HangulState.jung = jung;
            s_HangulState.jung1 = jung;
            s_HangulState.jung2 = -1;
            s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
        } else if (s_HangulState.jong == 0) {
            // 받침이 없는 상태에서 복모음 결합 시도 (예: ㅗ + ㅏ = ㅘ)
            if (s_HangulState.jung2 == -1) {
                int combinedJung = CombineJungseong(s_HangulState.jung1, jung);
                if (combinedJung != -1) {
                    s_HangulState.jung = combinedJung;
                    s_HangulState.jung2 = jung;
                    s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
                    return;
                }
            }
            // 결합 불가 모음이면 단독 모음 신규 입력
            s_HangulState.Reset();
            s_HangulState.jung = jung;
            s_HangulState.jung1 = jung;
            s_PendingHangulOps.push_back({ false, false, s_HangulState.GetCurrentChar() });
        } else {
            // 받침이 있는 상태에서 모음이 들어오면 연음 법칙 발동! (예: 잩 + ㅗ -> 자토, 닭 + ㅏ -> 달가)
            if (s_HangulState.jong2_cho != -1) {
                // 겹받침 연음: 앞 글자는 첫 받침만 남기고, 두 번째 받침이 새 글자의 초성으로 이동
                int carryCho = s_HangulState.jong2_cho;
                s_HangulState.jong = s_HangulState.jong1;
                s_HangulState.jong2_cho = -1;
                wchar_t prevSyllable = s_HangulState.GetCurrentChar();

                s_HangulState.Reset();
                s_HangulState.cho = carryCho;
                s_HangulState.jung = jung;
                s_HangulState.jung1 = jung;
                wchar_t nextSyllable = s_HangulState.GetCurrentChar();

                s_PendingHangulOps.push_back({ true, false, prevSyllable });
                s_PendingHangulOps.push_back({ false, false, nextSyllable });
            } else {
                // 단일 받침 연음: 앞 글자 받침을 제거하고, 받침이 새 글자의 초성으로 이동
                int carryCho = s_HangulState.jong1_cho;
                s_HangulState.jong = 0;
                s_HangulState.jong1 = 0;
                s_HangulState.jong1_cho = -1;
                wchar_t prevSyllable = s_HangulState.GetCurrentChar();

                s_HangulState.Reset();
                s_HangulState.cho = carryCho;
                s_HangulState.jung = jung;
                s_HangulState.jung1 = jung;
                wchar_t nextSyllable = s_HangulState.GetCurrentChar();

                s_PendingHangulOps.push_back({ true, false, prevSyllable });
                s_PendingHangulOps.push_back({ false, false, nextSyllable });
            }
        }
    }
}

void UI::OnHangulBackspace() {
    if (!s_HangulState.IsComposing()) return;

    if (s_HangulState.jong != 0) {
        if (s_HangulState.jong2_cho != -1) {
            // 겹받침 -> 단일 받침으로 복원
            s_HangulState.jong = s_HangulState.jong1;
            s_HangulState.jong2_cho = -1;
            s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
        } else {
            // 단일 받침 -> 받침 제거
            s_HangulState.jong = 0;
            s_HangulState.jong1 = 0;
            s_HangulState.jong1_cho = -1;
            s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
        }
    } else if (s_HangulState.jung != -1) {
        if (s_HangulState.jung2 != -1) {
            // 복모음 -> 단모음으로 복원
            s_HangulState.jung = s_HangulState.jung1;
            s_HangulState.jung2 = -1;
            s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
        } else if (s_HangulState.cho != -1) {
            // 초성+단모음 -> 초성만 남김
            s_HangulState.jung = -1;
            s_HangulState.jung1 = -1;
            s_PendingHangulOps.push_back({ true, false, s_HangulState.GetCurrentChar() });
        } else {
            // 단독 모음 -> 글자 삭제 및 조합 종료
            s_HangulState.Reset();
            s_PendingHangulOps.push_back({ false, true, 0 });
        }
    } else if (s_HangulState.cho != -1) {
        // 단독 초성 -> 글자 삭제 및 조합 종료
        s_HangulState.Reset();
        s_PendingHangulOps.push_back({ false, true, 0 });
    }
}

int UI::HangulInputTextCallback(ImGuiInputTextCallbackData* data) {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways) {
        if (s_LastActiveInputId != data->ID) {
            s_LastActiveInputId = data->ID;
            // 다른 입력창으로 포커스가 이동한 경우 이전 창 글자를 지우지 않도록 방어
            for (auto& op : s_PendingHangulOps) {
                op.replacePrevChar = false;
                op.deletePrevCharOnly = false;
            }
        }

        if (!s_PendingHangulOps.empty()) {
            for (const auto& op : s_PendingHangulOps) {
                if ((op.replacePrevChar || op.deletePrevCharOnly) && data->CursorPos > 0) {
                    int pos = data->CursorPos - 1;
                    while (pos > 0 && ((unsigned char)data->Buf[pos] & 0xC0) == 0x80) {
                        pos--;
                    }
                    data->DeleteChars(pos, data->CursorPos - pos);
                } else if (!op.replacePrevChar && !op.deletePrevCharOnly && data->HasSelection()) {
                    int selStart = (data->SelectionStart < data->SelectionEnd) ? data->SelectionStart : data->SelectionEnd;
                    int selEnd = (data->SelectionStart > data->SelectionEnd) ? data->SelectionStart : data->SelectionEnd;
                    data->DeleteChars(selStart, selEnd - selStart);
                }

                if (op.newChar != 0) {
                    char utf8[8] = {0};
                    WideCharToMultiByte(CP_UTF8, 0, &op.newChar, 1, utf8, sizeof(utf8), NULL, NULL);
                    data->InsertChars(data->CursorPos, utf8);
                }
            }
            s_PendingHangulOps.clear();
        }
    }
    return 0;
}

bool UI::InputTextKorean(const char* label, char* buf, size_t bufSize, ImGuiInputTextFlags flags) {
    return ImGui::InputText(label, buf, bufSize, flags | ImGuiInputTextFlags_CallbackAlways, &UI::HangulInputTextCallback);
}

bool UI::InputTextWithHintKorean(const char* label, const char* hint, char* buf, size_t bufSize, ImGuiInputTextFlags flags) {
    return ImGui::InputTextWithHint(label, hint, buf, bufSize, flags | ImGuiInputTextFlags_CallbackAlways, &UI::HangulInputTextCallback);
}

void UI::ShowToast(const std::string& msg, float durationSec) {
    s_ToastMessage = msg;
    s_ToastTimer = durationSec;
}

void UI::SortLibrary(int columnIdx, bool ascending) {
    std::stable_sort(s_Library.begin(), s_Library.end(), [columnIdx, ascending](const RecordingItem& a, const RecordingItem& b) {
        int cmp = 0;
        switch (columnIdx) {
        case 0: // 제목
            cmp = a.title.compare(b.title);
            break;
        case 1: // 캐릭터 (동일 캐릭터면 제목순)
            cmp = a.charName.compare(b.charName);
            if (cmp == 0) cmp = a.title.compare(b.title);
            break;
        case 2: // 길이(프레임)
            if (a.totalFrames != b.totalFrames)
                return ascending ? (a.totalFrames < b.totalFrames) : (a.totalFrames > b.totalFrames);
            cmp = a.title.compare(b.title);
            break;
        case 3: // 메모
            cmp = a.note.compare(b.note);
            break;
        case 4: // 등록일
        default:
            cmp = a.createdAt.compare(b.createdAt);
            break;
        }
        return ascending ? (cmp < 0) : (cmp > 0);
    });
}

void UI::SortSetLibrary(int columnIdx, bool ascending) {
    std::stable_sort(s_SetLibrary.begin(), s_SetLibrary.end(), [columnIdx, ascending](const RecordingSet& a, const RecordingSet& b) {
        int cmp = 0;
        switch (columnIdx) {
        case 0: // 세트 제목
            cmp = a.title.compare(b.title);
            break;
        case 1: // 캐릭터 (동일 캐릭터면 제목순)
            cmp = a.charName.compare(b.charName);
            if (cmp == 0) cmp = a.title.compare(b.title);
            break;
        case 2: // 슬롯 수
            if (a.GetActiveSlotCount() != b.GetActiveSlotCount())
                return ascending ? (a.GetActiveSlotCount() < b.GetActiveSlotCount()) : (a.GetActiveSlotCount() > b.GetActiveSlotCount());
            cmp = a.title.compare(b.title);
            break;
        case 3: // 메모
            cmp = a.note.compare(b.note);
            break;
        case 4: // 등록일
        default:
            cmp = a.createdAt.compare(b.createdAt);
            break;
        }
        return ascending ? (cmp < 0) : (cmp > 0);
    });
}

bool UI::ApplySetToGameSlots(const RecordingSet& set) {
    if (!GameMemory::IsPracticeMode()) return false;

    bool ok = true;
    for (int s = 0; s < Offsets::MaxSlots; ++s) {
        if (set.slots[s].enabled && !set.slots[s].item.inputs.empty()) {
            GGSTRecordingSlot slotData = RecordingManager::RecordingItemToSlot(set.slots[s].item);
            if (!GameMemory::WriteSlot(s, slotData)) {
                ok = false;
            }
        } else {
            // 세트에서 비워둔 슬롯은 인게임 슬롯 메모리도 깨끗이 비워서 랜덤 재생 시 섞이지 않도록 처리
            if (!GameMemory::ClearSlot(s)) {
                ok = false;
            }
        }
    }
    return ok;
}

void UI::OpenSetModalFromCurrentGameSlots() {
    s_EditingSetIndex = -1;
    s_EditingSetDraft = RecordingSet{};
    s_SetTitleBuf[0] = '\0';
    s_SetNoteBuf[0] = '\0';

    bool isPractice = GameMemory::IsPracticeMode();
    for (int s = 0; s < Offsets::MaxSlots; ++s) {
        GGSTRecordingSlot slot{};
        if (isPractice && GameMemory::ReadSlot(s, slot) && slot.bHasData != 0 && slot.TotalFrameCount > 4) {
            std::string defaultSlotTitle = "슬롯 " + std::to_string(s + 1) + " 행동";
            s_EditingSetDraft.slots[s].enabled = true;
            s_EditingSetDraft.slots[s].item = RecordingManager::SlotToRecordingItem(
                s, slot, defaultSlotTitle, s_SetCharBuf, "");
            strncpy_s(s_SetSlotTitleBufs[s], defaultSlotTitle.c_str(), _TRUNCATE);
        } else {
            s_EditingSetDraft.slots[s].enabled = false;
            s_SetSlotTitleBufs[s][0] = '\0';
        }
    }

    ResetHangulState();
    s_ShowSetModal = true;
}

void UI::SetupTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 1.5f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 8);
    style.CellPadding = ImVec2(8, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.11f, 0.14f, 0.95f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.14f, 0.14f, 0.18f, 0.85f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.13f, 0.17f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.35f, 0.33f, 0.42f, 0.70f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.18f, 0.18f, 0.24f, 0.85f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.26f, 0.24f, 0.32f, 0.95f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.32f, 0.28f, 0.40f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.60f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.80f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.40f, 0.10f, 0.10f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.35f, 0.35f, 0.42f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.48f, 0.48f, 0.58f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.95f, 0.30f, 0.25f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.85f, 0.25f, 0.20f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.98f, 0.35f, 0.30f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.70f, 0.20f, 0.18f, 0.85f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.88f, 0.28f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.98f, 0.35f, 0.32f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.70f, 0.20f, 0.18f, 0.50f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.80f, 0.25f, 0.22f, 0.70f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.90f, 0.30f, 0.28f, 0.90f);
    colors[ImGuiCol_Separator]             = ImVec4(0.35f, 0.35f, 0.42f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.75f, 0.35f, 0.30f, 0.90f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.95f, 0.30f, 0.25f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.80f, 0.25f, 0.22f, 0.45f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.90f, 0.30f, 0.25f, 0.80f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(1.00f, 0.35f, 0.30f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.22f, 0.20f, 0.28f, 0.90f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.78f, 0.25f, 0.22f, 0.95f);
    colors[ImGuiCol_TabSelected]           = ImVec4(0.72f, 0.20f, 0.18f, 1.00f);
}

void UI::Initialize() {
    SetupTheme();

    // 한글 폰트 로드 시도 (윈도우 설치 드라이브에 무관하게 %WINDIR%\Fonts 경로 동적 탐색)
    ImGuiIO& io = ImGui::GetIO();
    char winDir[MAX_PATH] = {0};
    std::string baseFontDir = "C:\\Windows\\Fonts\\";
    if (GetWindowsDirectoryA(winDir, MAX_PATH) > 0) {
        baseFontDir = std::string(winDir) + "\\Fonts\\";
    }

    std::vector<std::string> fontPaths = {
        baseFontDir + "malgun.ttf",
        baseFontDir + "gulim.ttc",
        baseFontDir + "segoeui.ttf",
        "C:\\Windows\\Fonts\\malgun.ttf",
        "C:\\Windows\\Fonts\\gulim.ttc"
    };

    for (const auto& path : fontPaths) {
        FILE* f = nullptr;
        if (fopen_s(&f, path.c_str(), "rb") == 0 && f) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF(path.c_str(), 16.5f, NULL, io.Fonts->GetGlyphRangesKorean());
            break;
        }
    }

    // 로컬 개별 보관함 및 세트 보관함 파일 로드
    RecordingManager::LoadRecordingLibrary(s_Library);
    RecordingManager::LoadRecordingSetLibrary(s_SetLibrary);
}

void UI::RenderHeader() {
    bool isPractice = GameMemory::IsPracticeMode();

    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "[GGST] 프랙티스 녹화 공유기 (만든이 : 남양주빌런)");
    ImGui::SameLine();

    // 프랙티스 모드 상태 배지
    if (isPractice) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "[프랙티스 활성]");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "[프랙티스 아님 (안전 잠금)]");
    }

    ImGui::TextColored(ImVec4(0.65f, 0.85f, 1.0f, 1.0f), "제미나이4 아르곤(Gemini 4 Argon)을 활용해 만든 모드입니다.");
    ImGui::SameLine();
    if (isPractice) {
        ImGui::TextDisabled("| 창 모서리와 표 구분선을 마우스로 드래그해 크기를 조절할 수 있습니다.");
    } else {
        ImGui::TextDisabled("| 프랙티스(Training) 모드 진입 시 슬롯 읽기/주입 기능이 활성화됩니다.");
    }
    ImGui::EndGroup();

    // 단축키 변경 버튼 (우측 상단 배치)
    ImGui::SameLine(ImGui::GetWindowWidth() - 250);
    if (DX11Hook::IsWaitingForHotkey()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.65f, 0.1f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button("키를 누르세요... (ESC: 취소)", ImVec2(235, 28))) {
            DX11Hook::CancelWaitingForHotkey();
        }
        ImGui::PopStyleColor(2);
    } else {
        std::string btnText = "열기 단축키 변경: [" + Config::GetToggleKeyName() + "]";
        if (ImGui::Button(btnText.c_str(), ImVec2(235, 28))) {
            DX11Hook::StartWaitingForHotkey();
        }
    }

    ImGui::Separator();

    // 퀵 액션 버튼 바 (단일 코드 GGSTREC1: 과 세트 코드 GGSTSET1: 모두 자동 인식!)
    if (ImGui::Button("클립보드에서 코드 붙여넣기 (개별/세트)", ImVec2(250, 32))) {
        std::string clipText = RecordingManager::GetClipboardText();
        std::string err;
        if (RecordingManager::IsSetShareString(clipText)) {
            if (RecordingManager::ImportSetFromShareString(clipText, s_PendingConfirmSet, err)) {
                s_ShowConfirmSetModal = true;
            } else {
                ShowToast("세트 붙여넣기 실패: " + err, 4.0f);
            }
        } else {
            if (RecordingManager::ImportFromShareString(clipText, s_PendingConfirmItem, err)) {
                s_ShowConfirmModal = true;
                s_ConfirmTargetSlot = 0; // 기본 1번 슬롯
            } else {
                ShowToast("붙여넣기 실패: " + err, 4.0f);
            }
        }
    }

    ImGui::SameLine();
    std::string libBtnText = (s_ShowLibrary && s_ActiveTab == 0)
        ? ("개별 보관함 열림 (" + std::to_string(s_Library.size()) + ")")
        : ("개별 보관함 탭 (" + std::to_string(s_Library.size()) + ")");
    if (ImGui::Button(libBtnText.c_str(), ImVec2(155, 32))) {
        if (s_ShowLibrary && s_ActiveTab == 0) {
            s_ShowLibrary = false;
        } else {
            s_ShowLibrary = true;
            s_ActiveTab = 0;
            s_RequestedTab = 0;
        }
    }

    ImGui::SameLine();
    std::string setBtnText = (s_ShowLibrary && s_ActiveTab == 1)
        ? ("세트 보관함 열림 (" + std::to_string(s_SetLibrary.size()) + ")")
        : ("세트 보관함 탭 (" + std::to_string(s_SetLibrary.size()) + ")");
    if (ImGui::Button(setBtnText.c_str(), ImVec2(155, 32))) {
        if (s_ShowLibrary && s_ActiveTab == 1) {
            s_ShowLibrary = false;
        } else {
            s_ShowLibrary = true;
            s_ActiveTab = 1;
            s_RequestedTab = 1;
        }
    }

    // 한/영 상태 표시 및 전환 버튼
    ImGui::SameLine();
    if (s_KoreanMode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.32f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.62f, 0.40f, 1.00f));
        if (ImGui::Button("입력: [한글] (한/영키)", ImVec2(165, 32))) {
            ToggleKoreanMode();
        }
        ImGui::PopStyleColor(2);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.58f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.32f, 0.45f, 0.72f, 1.00f));
        if (ImGui::Button("입력: [영문] (한/영키)", ImVec2(165, 32))) {
            ToggleKoreanMode();
        }
        ImGui::PopStyleColor(2);
    }

    // 토스트 알림 렌더링
    if (s_ToastTimer > 0.0f) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "[알림] %s", s_ToastMessage.c_str());
        s_ToastTimer -= ImGui::GetIO().DeltaTime;
    }

    ImGui::Separator();
}

void UI::RenderSlots() {
    bool isPractice = GameMemory::IsPracticeMode();

    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "인게임 트레이닝 내장 녹화 슬롯 (Slot 1 ~ 5)");

    // 우측 상단에 [현재 슬롯 1~5 통째로 세트 만들기] 및 [새 세트 구성하기] 버튼 배치
    float headerBtnX = ImGui::GetWindowWidth() - 410.0f;
    if (headerBtnX > 330.0f) {
        ImGui::SameLine(headerBtnX);
    } else {
        ImGui::SameLine();
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.28f, 0.68f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.36f, 0.82f, 1.00f));
    if (ImGui::Button("현재 슬롯 1~5 묶어서 세트 만들기", ImVec2(230, 26))) {
        OpenSetModalFromCurrentGameSlots();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::BeginDisabled(!isPractice);
    if (ImGui::Button("전체 슬롯 비우기", ImVec2(155, 26))) {
        for (int s = 0; s < Offsets::MaxSlots; ++s) {
            GameMemory::ClearSlot(s);
        }
        ShowToast("인게임 슬롯 1 ~ 5를 모두 비웠습니다.");
    }
    ImGui::EndDisabled();

    ImGui::Spacing();

    for (int i = 0; i < Offsets::MaxSlots; ++i) {
        ImGui::PushID(i);

        GGSTRecordingSlot slot{};
        bool hasRead = isPractice && GameMemory::ReadSlot(i, slot);
        bool hasData = hasRead && (slot.bHasData != 0) && (slot.TotalFrameCount > 4);

        int duration = 0;
        float durationSec = 0.0f;
        if (hasData) {
            duration = slot.TotalFrameCount - slot.StartOffsetFrame;
            if (duration <= 0) duration = slot.TotalFrameCount;
            durationSec = (float)duration / 60.0f;
        }

        // 각 슬롯 박스 렌더링
        ImGui::BeginChild("SlotBox", ImVec2(0, 44), true, ImGuiWindowFlags_NoScrollbar);

        // 슬롯 번호 및 상태
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "슬롯 %d", i + 1);
        ImGui::SameLine(75);

        if (!isPractice) {
            ImGui::TextDisabled("프랙티스 모드 진입 대기 중...");
        } else if (hasData) {
            const char* facingStr = (slot.RecordedFacingDir == 0) ? "우(1P)" : "좌(2P)";
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[데이터 있음]");
            ImGui::SameLine();
            ImGui::Text("%d 프레임 (%.2f초) | 녹화 기준 시선: %s", duration, durationSec, facingStr);
        } else {
            ImGui::TextDisabled("[비어 있음]");
        }

        // 우측 액션 버튼들
        float rightOffset = ImGui::GetWindowWidth() - 260.0f;
        if (rightOffset > 320.0f) {
            ImGui::SameLine(rightOffset);
        } else {
            ImGui::SameLine();
        }

        // 1. 저장 및 공유 코드 생성 버튼 (제목/캐릭터/메모 직접 작성 모달 오픈)
        ImGui::BeginDisabled(!hasData);
        if (ImGui::Button("저장 / 공유 코드 만들기", ImVec2(160, 26))) {
            s_ExportSourceSlot = i;
            s_EditingLibraryIndex = -1;
            snprintf(s_ExportTitleBuf, sizeof(s_ExportTitleBuf), "");
            snprintf(s_ExportNoteBuf, sizeof(s_ExportNoteBuf), "");
            ResetHangulState();
            s_ShowExportModal = true;
        }
        ImGui::EndDisabled();

        // 2. 슬롯 비우기 버튼
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasData);
        if (ImGui::Button("비우기", ImVec2(75, 26))) {
            if (GameMemory::ClearSlot(i)) {
                ShowToast("슬롯 " + std::to_string(i + 1) + " 메모리를 비웠습니다.");
            }
        }
        ImGui::EndDisabled();

        ImGui::EndChild();
        ImGui::PopID();
    }
}

void UI::RenderConfirmModal() {
    static char s_ConfirmTitleEdit[256] = {0};
    static char s_ConfirmCharEdit[128] = {0};
    static char s_ConfirmNoteEdit[512] = {0};

    if (s_ShowConfirmModal) {
        ResetHangulState();
        strncpy_s(s_ConfirmTitleEdit, s_PendingConfirmItem.title.c_str(), _TRUNCATE);
        strncpy_s(s_ConfirmCharEdit, s_PendingConfirmItem.charName.c_str(), _TRUNCATE);
        strncpy_s(s_ConfirmNoteEdit, s_PendingConfirmItem.note.c_str(), _TRUNCATE);
        ImGui::OpenPopup("녹화 데이터 인식 확인##ConfirmModal");
        s_ShowConfirmModal = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(580, 440), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("녹화 데이터 인식 확인##ConfirmModal", NULL, ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "클립보드에서 녹화 공유 코드를 인식했습니다! (이거 맞아?)");
        ImGui::TextDisabled("아래 정보를 확인(필요시 한글/영문 직접 수정 가능)한 뒤 원하는 슬롯에 적용하세요.");
        ImGui::Separator();

        // 1. 인식된 정보 확인 및 직접 편집 가능 영역
        ImGui::BeginChild("ConfirmInfoBox", ImVec2(0, 210), true);

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "녹화 제목:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (InputTextKorean("##ConfirmTitle", s_ConfirmTitleEdit, sizeof(s_ConfirmTitleEdit))) {
            s_PendingConfirmItem.title = s_ConfirmTitleEdit;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "캐릭터:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (InputTextKorean("##ConfirmChar", s_ConfirmCharEdit, sizeof(s_ConfirmCharEdit))) {
            s_PendingConfirmItem.charName = s_ConfirmCharEdit;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "메모/조건:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (InputTextKorean("##ConfirmNote", s_ConfirmNoteEdit, sizeof(s_ConfirmNoteEdit))) {
            s_PendingConfirmItem.note = s_ConfirmNoteEdit;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 재생 길이 & 기준 시선
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "녹화 길이:");
        ImGui::SameLine(100);
        ImGui::Text("%d 프레임 (%.2f 초)", s_PendingConfirmItem.totalFrames, s_PendingConfirmItem.GetDurationSeconds());

        ImGui::SameLine(300);
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "기준 시선:");
        ImGui::SameLine();
        ImGui::Text("%s", (s_PendingConfirmItem.facingDir == 0) ? "우(1P 방향)" : "좌(2P 방향)");

        ImGui::EndChild();

        // 2. 적용할 슬롯 선택
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "적용할 프랙티스 슬롯을 선택하세요:");
        for (int i = 0; i < Offsets::MaxSlots; ++i) {
            std::string label = "슬롯 " + std::to_string(i + 1);
            if (ImGui::RadioButton(label.c_str(), s_ConfirmTargetSlot == i)) {
                s_ConfirmTargetSlot = i;
            }
            if (i < Offsets::MaxSlots - 1) ImGui::SameLine(0, 18);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 3. 확인 / 취소 버튼
        bool isPractice = GameMemory::IsPracticeMode();

        ImGui::BeginDisabled(!isPractice);
        if (ImGui::Button("맞음 (보관함 저장 + 슬롯 적용)", ImVec2(215, 34))) {
            s_PendingConfirmItem.title = (std::strlen(s_ConfirmTitleEdit) > 0) ? s_ConfirmTitleEdit : "이름 없는 녹화";
            s_PendingConfirmItem.charName = (std::strlen(s_ConfirmCharEdit) > 0) ? s_ConfirmCharEdit : "미지정";
            s_PendingConfirmItem.note = s_ConfirmNoteEdit;

            GGSTRecordingSlot slot = RecordingManager::RecordingItemToSlot(s_PendingConfirmItem);
            if (GameMemory::WriteSlot(s_ConfirmTargetSlot, slot)) {
                s_Library.push_back(s_PendingConfirmItem);
                SortLibrary(1, true); // 캐릭터 -> 제목 순 자동 정렬
                RecordingManager::SaveRecordingLibrary(s_Library);
                ShowToast("슬롯 " + std::to_string(s_ConfirmTargetSlot + 1) + " 적용 및 보관함 저장 완료!");
            } else {
                ShowToast("슬롯 메모리 주입에 실패했습니다.");
            }
            ResetHangulState();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(!isPractice);
        if (ImGui::Button("슬롯에만 적용", ImVec2(140, 34))) {
            s_PendingConfirmItem.title = (std::strlen(s_ConfirmTitleEdit) > 0) ? s_ConfirmTitleEdit : "이름 없는 녹화";
            s_PendingConfirmItem.charName = (std::strlen(s_ConfirmCharEdit) > 0) ? s_ConfirmCharEdit : "미지정";
            s_PendingConfirmItem.note = s_ConfirmNoteEdit;

            GGSTRecordingSlot slot = RecordingManager::RecordingItemToSlot(s_PendingConfirmItem);
            if (GameMemory::WriteSlot(s_ConfirmTargetSlot, slot)) {
                ShowToast("슬롯 " + std::to_string(s_ConfirmTargetSlot + 1) + "에 녹화가 적용되었습니다!");
            }
            ResetHangulState();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(130, 34))) {
            ResetHangulState();
            ImGui::CloseCurrentPopup();
        }

        if (!isPractice) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "※ 프랙티스 모드 상태에서만 슬롯 적용이 가능합니다.");
        }

        ImGui::EndPopup();
    }
}

void UI::RenderExportModal() {
    if (s_ShowExportModal) {
        ResetHangulState();
        ImGui::OpenPopup("녹화 정보 입력 (저장 및 공유 코드 생성)##ExportModal");
        s_ShowExportModal = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 370), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("녹화 정보 입력 (저장 및 공유 코드 생성)##ExportModal", NULL, ImGuiWindowFlags_NoMove)) {
        if (s_EditingLibraryIndex >= 0 && s_EditingLibraryIndex < (int)s_Library.size()) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "보관함 녹화 정보 수정 (수정 시 공유 코드도 새 정보로 갱신됩니다)");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "슬롯 %d 녹화 정보 입력 (공유 코드에 아래 내용이 함께 담깁니다)", s_ExportSourceSlot + 1);
        }

        // 모달 상단 한/영 전환 표시줄
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("키보드의 [한/영] 키(또는 Shift+Space)로 한글/영문을 전환할 수 있습니다:");
        ImGui::SameLine();
        if (s_KoreanMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.32f, 0.95f));
            if (ImGui::Button("현재: [한글 입력] (클릭 시 영문)", ImVec2(185, 24))) {
                ToggleKoreanMode();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.58f, 0.95f));
            if (ImGui::Button("현재: [영문 입력] (클릭 시 한글)", ImVec2(185, 24))) {
                ToggleKoreanMode();
            }
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // 1. 캐릭터 이름 직접 입력 + 빠른 선택 도우미
        ImGui::AlignTextToFramePadding();
        ImGui::Text("캐릭터 이름:");
        ImGui::SameLine(110);
        ImGui::SetNextItemWidth(230);
        InputTextWithHintKorean("##ExportChar", "예: 자토, 솔, 카이 (직접 입력)", s_ExportCharBuf, sizeof(s_ExportCharBuf));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        if (ImGui::BeginCombo("##QuickCharCombo", "캐릭터 목록에서 빠른 선택")) {
            for (const auto& c : CharacterTable) {
                if (ImGui::Selectable(c.nameKo)) {
                    ResetHangulState();
                    strncpy_s(s_ExportCharBuf, c.nameKo, _TRUNCATE);
                }
            }
            ImGui::EndCombo();
        }

        // 2. 녹화 제목 직접 입력
        ImGui::AlignTextToFramePadding();
        ImGui::Text("녹화 제목:");
        ImGui::SameLine(110);
        ImGui::SetNextItemWidth(-1);
        InputTextWithHintKorean("##ExportTitle", "예: 자토 구석 다운후 중하단 이지선다 1번(하단)", s_ExportTitleBuf, sizeof(s_ExportTitleBuf));

        // 3. 메모 / 시작 조건 직접 입력
        ImGui::AlignTextToFramePadding();
        ImGui::Text("메모 / 조건:");
        ImGui::SameLine(110);
        ImGui::SetNextItemWidth(-1);
        InputTextWithHintKorean("##ExportNote", "예: 구석 시작 / 근S 가드 후 셋업 등 자유 기재", s_ExportNoteBuf, sizeof(s_ExportNoteBuf));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 보관함 기존 항목 수정 모드인 경우
        if (s_EditingLibraryIndex >= 0 && s_EditingLibraryIndex < (int)s_Library.size()) {
            if (ImGui::Button("수정 저장 + 새 공유 코드 복사", ImVec2(235, 36))) {
                auto& item = s_Library[s_EditingLibraryIndex];
                item.charName = (std::strlen(s_ExportCharBuf) > 0) ? s_ExportCharBuf : "미지정";
                item.title = (std::strlen(s_ExportTitleBuf) > 0) ? s_ExportTitleBuf : "이름 없는 녹화";
                item.note = s_ExportNoteBuf;

                std::string shareStr = RecordingManager::ExportToShareString(item);
                RecordingManager::SetClipboardText(shareStr);
                SortLibrary(1, true);
                RecordingManager::SaveRecordingLibrary(s_Library);
                ShowToast("녹화 정보가 수정되고 새 공유 코드가 클립보드에 복사되었습니다!");

                ResetHangulState();
                s_EditingLibraryIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("수정만 저장", ImVec2(140, 36))) {
                auto& item = s_Library[s_EditingLibraryIndex];
                item.charName = (std::strlen(s_ExportCharBuf) > 0) ? s_ExportCharBuf : "미지정";
                item.title = (std::strlen(s_ExportTitleBuf) > 0) ? s_ExportTitleBuf : "이름 없는 녹화";
                item.note = s_ExportNoteBuf;

                SortLibrary(1, true);
                RecordingManager::SaveRecordingLibrary(s_Library);
                ShowToast("보관함 녹화 정보가 수정되었습니다!");

                ResetHangulState();
                s_EditingLibraryIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("취소", ImVec2(120, 36))) {
                ResetHangulState();
                s_EditingLibraryIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        } else {
            // 슬롯에서 신규 저장 / 코드 생성 모드
            std::string finalChar = (std::strlen(s_ExportCharBuf) > 0) ? s_ExportCharBuf : "미지정";
            std::string finalTitle = (std::strlen(s_ExportTitleBuf) > 0)
                ? s_ExportTitleBuf
                : ("슬롯 " + std::to_string(s_ExportSourceSlot + 1) + " 녹화");
            std::string finalNote = s_ExportNoteBuf;

            if (ImGui::Button("보관함 저장 + 코드 복사", ImVec2(200, 36))) {
                GGSTRecordingSlot slot{};
                if (GameMemory::ReadSlot(s_ExportSourceSlot, slot)) {
                    RecordingItem item = RecordingManager::SlotToRecordingItem(
                        s_ExportSourceSlot, slot, finalTitle, finalChar, finalNote);

                    s_Library.push_back(item);
                    SortLibrary(1, true); // 캐릭터 -> 제목 순 자동 정렬
                    RecordingManager::SaveRecordingLibrary(s_Library);

                    std::string shareStr = RecordingManager::ExportToShareString(item);
                    RecordingManager::SetClipboardText(shareStr);
                    ShowToast("보관함에 저장되고 공유 코드가 클립보드에 복사되었습니다!");
                }
                ResetHangulState();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("코드만 복사", ImVec2(120, 36))) {
                GGSTRecordingSlot slot{};
                if (GameMemory::ReadSlot(s_ExportSourceSlot, slot)) {
                    RecordingItem item = RecordingManager::SlotToRecordingItem(
                        s_ExportSourceSlot, slot, finalTitle, finalChar, finalNote);
                    std::string shareStr = RecordingManager::ExportToShareString(item);
                    if (RecordingManager::SetClipboardText(shareStr)) {
                        ShowToast("공유 코드가 클립보드에 복사되었습니다!");
                    }
                }
                ResetHangulState();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("보관함만 저장", ImVec2(120, 36))) {
                GGSTRecordingSlot slot{};
                if (GameMemory::ReadSlot(s_ExportSourceSlot, slot)) {
                    RecordingItem item = RecordingManager::SlotToRecordingItem(
                        s_ExportSourceSlot, slot, finalTitle, finalChar, finalNote);
                    s_Library.push_back(item);
                    SortLibrary(1, true);
                    RecordingManager::SaveRecordingLibrary(s_Library);
                    ShowToast("보관함에 저장되었습니다!");
                }
                ResetHangulState();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("취소", ImVec2(80, 36))) {
                ResetHangulState();
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
    }
}

void UI::RenderConfirmSetModal() {
    static char s_ConfirmSetTitleEdit[256] = {0};
    static char s_ConfirmSetCharEdit[128] = {0};
    static char s_ConfirmSetNoteEdit[512] = {0};

    if (s_ShowConfirmSetModal) {
        ResetHangulState();
        strncpy_s(s_ConfirmSetTitleEdit, s_PendingConfirmSet.title.c_str(), _TRUNCATE);
        strncpy_s(s_ConfirmSetCharEdit, s_PendingConfirmSet.charName.c_str(), _TRUNCATE);
        strncpy_s(s_ConfirmSetNoteEdit, s_PendingConfirmSet.note.c_str(), _TRUNCATE);
        ImGui::OpenPopup("녹화 세트 코드 인식 확인##ConfirmSetModal");
        s_ShowConfirmSetModal = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("녹화 세트 코드 인식 확인##ConfirmSetModal", NULL, ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "클립보드에서 [녹화 세트] 공유 코드를 인식했습니다! (이거 맞아?)");
        ImGui::TextDisabled("적용 시 슬롯 1~5가 아래 세트 구성으로 한 번에 교체되며, 비어 있는 슬롯은 자동으로 비워집니다.");
        ImGui::Separator();

        ImGui::BeginChild("ConfirmSetInfoBox", ImVec2(0, 320), true);

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "세트 제목:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (InputTextKorean("##ConfirmSetTitle", s_ConfirmSetTitleEdit, sizeof(s_ConfirmSetTitleEdit))) {
            s_PendingConfirmSet.title = s_ConfirmSetTitleEdit;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "캐릭터:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (InputTextKorean("##ConfirmSetChar", s_ConfirmSetCharEdit, sizeof(s_ConfirmSetCharEdit))) {
            s_PendingConfirmSet.charName = s_ConfirmSetCharEdit;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.8f, 0.85f, 1.0f, 1.0f), "메모/조건:");
        ImGui::SameLine(100);
        ImGui::SetNextItemWidth(-1);
        if (InputTextKorean("##ConfirmSetNote", s_ConfirmSetNoteEdit, sizeof(s_ConfirmSetNoteEdit))) {
            s_PendingConfirmSet.note = s_ConfirmSetNoteEdit;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "세트 슬롯 구성 (총 %d개 슬롯 활성):", s_PendingConfirmSet.GetActiveSlotCount());
        ImGui::Spacing();

        for (int s = 0; s < Offsets::MaxSlots; ++s) {
            const auto& slotEntry = s_PendingConfirmSet.slots[s];
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "  - 슬롯 %d:", s + 1);
            ImGui::SameLine(95);
            if (slotEntry.enabled && !slotEntry.item.inputs.empty()) {
                ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.5f, 1.0f), "[채움] %s", slotEntry.item.title.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(%d 프레임 / %.2f초)", slotEntry.item.totalFrames, slotEntry.item.GetDurationSeconds());
            } else {
                ImGui::TextDisabled("[비워두기 - 적용 시 슬롯 자동 비움]");
            }
        }

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool isPractice = GameMemory::IsPracticeMode();

        ImGui::BeginDisabled(!isPractice);
        if (ImGui::Button("맞음 (세트 보관함 저장 + 슬롯 1~5 일괄 적용)", ImVec2(270, 36))) {
            s_PendingConfirmSet.title = (std::strlen(s_ConfirmSetTitleEdit) > 0) ? s_ConfirmSetTitleEdit : "이름 없는 세트";
            s_PendingConfirmSet.charName = (std::strlen(s_ConfirmSetCharEdit) > 0) ? s_ConfirmSetCharEdit : "미지정";
            s_PendingConfirmSet.note = s_ConfirmSetNoteEdit;

            if (ApplySetToGameSlots(s_PendingConfirmSet)) {
                s_SetLibrary.push_back(s_PendingConfirmSet);
                SortSetLibrary(1, true);
                RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
                s_ShowLibrary = true;
                s_ActiveTab = 1;
                s_RequestedTab = 1;
                ShowToast("세트 보관함 저장 및 슬롯 1~5 일괄 적용 완료!");
            } else {
                ShowToast("슬롯 일괄 적용에 실패했습니다.");
            }
            ResetHangulState();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("세트 보관함에만 저장", ImVec2(165, 36))) {
            s_PendingConfirmSet.title = (std::strlen(s_ConfirmSetTitleEdit) > 0) ? s_ConfirmSetTitleEdit : "이름 없는 세트";
            s_PendingConfirmSet.charName = (std::strlen(s_ConfirmSetCharEdit) > 0) ? s_ConfirmSetCharEdit : "미지정";
            s_PendingConfirmSet.note = s_ConfirmSetNoteEdit;

            s_SetLibrary.push_back(s_PendingConfirmSet);
            SortSetLibrary(1, true);
            RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
            s_ShowLibrary = true;
            s_ActiveTab = 1;
            s_RequestedTab = 1;
            ShowToast("세트 보관함에 저장되었습니다!");

            ResetHangulState();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(110, 36))) {
            ResetHangulState();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void UI::RenderSetModal() {
    if (s_ShowSetModal) {
        ResetHangulState();
        ImGui::OpenPopup("녹화 세트 구성 및 저장 (슬롯 1 ~ 5 일괄 관리)##SetModal");
        s_ShowSetModal = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(760, 540), ImGuiCond_FirstUseEver);

    if (ImGui::BeginPopupModal("녹화 세트 구성 및 저장 (슬롯 1 ~ 5 일괄 관리)##SetModal", NULL, ImGuiWindowFlags_NoMove)) {
        if (s_EditingSetIndex >= 0 && s_EditingSetIndex < (int)s_SetLibrary.size()) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "세트 정보 및 슬롯 1~5 구성 수정");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "새 녹화 세트 만들기 (슬롯 1~5 조합 및 빈 슬롯 비움 설정)");
        }

        // 한/영 전환 표시줄
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("키보드의 [한/영] 키(또는 Shift+Space)로 한글/영문을 전환할 수 있습니다:");
        ImGui::SameLine();
        if (s_KoreanMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.32f, 0.95f));
            if (ImGui::Button("현재: [한글 입력] (클릭 시 영문)##SetKorBtn", ImVec2(185, 24))) {
                ToggleKoreanMode();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.35f, 0.58f, 0.95f));
            if (ImGui::Button("현재: [영문 입력] (클릭 시 한글)##SetEngBtn", ImVec2(185, 24))) {
                ToggleKoreanMode();
            }
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // 1. 캐릭터 이름
        ImGui::AlignTextToFramePadding();
        ImGui::Text("캐릭터 이름:");
        ImGui::SameLine(110);
        ImGui::SetNextItemWidth(250);
        InputTextWithHintKorean("##SetChar", "예: 자토, 솔, 카이 (직접 입력)", s_SetCharBuf, sizeof(s_SetCharBuf));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(220);
        if (ImGui::BeginCombo("##SetQuickCharCombo", "캐릭터 목록에서 빠른 선택")) {
            for (const auto& c : CharacterTable) {
                if (ImGui::Selectable(c.nameKo)) {
                    ResetHangulState();
                    strncpy_s(s_SetCharBuf, c.nameKo, _TRUNCATE);
                }
            }
            ImGui::EndCombo();
        }

        // 2. 세트 제목
        ImGui::AlignTextToFramePadding();
        ImGui::Text("세트 제목:");
        ImGui::SameLine(110);
        ImGui::SetNextItemWidth(-1);
        InputTextWithHintKorean("##SetTitle", "예: 자토 2K2D 후 샌드위치 정역중하단 세트", s_SetTitleBuf, sizeof(s_SetTitleBuf));

        // 3. 세트 메모 / 조건
        ImGui::AlignTextToFramePadding();
        ImGui::Text("세트 메모:");
        ImGui::SameLine(110);
        ImGui::SetNextItemWidth(-1);
        InputTextWithHintKorean("##SetNote", "예: 구석 2K-2D 다운 후 카운터/랜덤 재생 켜고 가드 연습", s_SetNoteBuf, sizeof(s_SetNoteBuf));

        ImGui::Spacing();
        ImGui::Separator();

        // 4. 슬롯 1 ~ 5 구성 섹션
        bool isPractice = GameMemory::IsPracticeMode();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "슬롯 1 ~ 5 구성 (비워둔 슬롯은 세트 적용 시 자동으로 비워집니다)");
        ImGui::SameLine(ImGui::GetWindowWidth() - 320);
        ImGui::BeginDisabled(!isPractice);
        if (ImGui::Button("현재 인게임 슬롯 1~5 그대로 가져오기", ImVec2(215, 24))) {
            for (int s = 0; s < Offsets::MaxSlots; ++s) {
                GGSTRecordingSlot slot{};
                if (GameMemory::ReadSlot(s, slot) && slot.bHasData != 0 && slot.TotalFrameCount > 4) {
                    std::string defName = "슬롯 " + std::to_string(s + 1) + " 행동";
                    s_EditingSetDraft.slots[s].enabled = true;
                    s_EditingSetDraft.slots[s].item = RecordingManager::SlotToRecordingItem(
                        s, slot, defName, s_SetCharBuf, "");
                    strncpy_s(s_SetSlotTitleBufs[s], defName.c_str(), _TRUNCATE);
                } else {
                    s_EditingSetDraft.slots[s].enabled = false;
                    s_SetSlotTitleBufs[s][0] = '\0';
                }
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("전체 비움", ImVec2(80, 24))) {
            for (int s = 0; s < Offsets::MaxSlots; ++s) {
                s_EditingSetDraft.slots[s].enabled = false;
                s_SetSlotTitleBufs[s][0] = '\0';
            }
        }

        ImGui::BeginChild("SetSlotsConfigBox", ImVec2(0, 225), true);
        for (int s = 0; s < Offsets::MaxSlots; ++s) {
            ImGui::PushID(s);
            auto& slotEntry = s_EditingSetDraft.slots[s];

            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "슬롯 %d", s + 1);
            ImGui::SameLine(65);

            // 현재 선택된 녹화 표시 문자열
            std::string previewLabel;
            if (!slotEntry.enabled || slotEntry.item.inputs.empty()) {
                previewLabel = "[비워두기 - 적용 시 이 슬롯 비움]";
            } else {
                previewLabel = "[" + std::to_string(slotEntry.item.totalFrames) + "f] " + slotEntry.item.title;
            }

            ImGui::SetNextItemWidth(330);
            if (ImGui::BeginCombo("##SlotSourceSelect", previewLabel.c_str())) {
                // 1) 비워두기 선택지
                if (ImGui::Selectable("[비워두기 - 적용 시 이 슬롯 비움]", !slotEntry.enabled)) {
                    slotEntry.enabled = false;
                    s_SetSlotTitleBufs[s][0] = '\0';
                }

                // 2) 현재 인게임 슬롯(1~5)에서 가져오기
                if (isPractice) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "[현재 인게임 프랙티스 슬롯에서 가져오기]");
                    for (int gameSlotIdx = 0; gameSlotIdx < Offsets::MaxSlots; ++gameSlotIdx) {
                        GGSTRecordingSlot liveSlot{};
                        if (GameMemory::ReadSlot(gameSlotIdx, liveSlot) && liveSlot.bHasData != 0 && liveSlot.TotalFrameCount > 4) {
                            int dur = liveSlot.TotalFrameCount - liveSlot.StartOffsetFrame;
                            if (dur <= 0) dur = liveSlot.TotalFrameCount;
                            std::string liveLabel = "현재 인게임 슬롯 " + std::to_string(gameSlotIdx + 1) +
                                                    " 데이터 (" + std::to_string(dur) + " 프레임)";
                            if (ImGui::Selectable(liveLabel.c_str())) {
                                std::string defTitle = "슬롯 " + std::to_string(s + 1) + " 행동";
                                slotEntry.enabled = true;
                                slotEntry.item = RecordingManager::SlotToRecordingItem(
                                    gameSlotIdx, liveSlot, defTitle, s_SetCharBuf, "");
                                strncpy_s(s_SetSlotTitleBufs[s], defTitle.c_str(), _TRUNCATE);
                            }
                        }
                    }
                }

                // 3) 로컬 개별 보관함(s_Library)에서 선택하기
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "[로컬 개별 보관함(Recordings.json)에서 선택]");
                if (s_Library.empty()) {
                    ImGui::TextDisabled("  (개별 보관함에 저장된 녹화가 없습니다)");
                } else {
                    for (int libIdx = 0; libIdx < (int)s_Library.size(); ++libIdx) {
                        const auto& libItem = s_Library[libIdx];
                        std::string itemLabel = "[" + libItem.charName + "] " + libItem.title +
                                                " (" + std::to_string(libItem.totalFrames) + "f)##Lib" + std::to_string(libIdx);
                        if (ImGui::Selectable(itemLabel.c_str())) {
                            slotEntry.enabled = true;
                            slotEntry.item = libItem;
                            strncpy_s(s_SetSlotTitleBufs[s], libItem.title.c_str(), _TRUNCATE);
                            if (std::strlen(s_SetCharBuf) == 0 && !libItem.charName.empty() && libItem.charName != "미지정") {
                                strncpy_s(s_SetCharBuf, libItem.charName.c_str(), _TRUNCATE);
                            }
                        }
                    }
                }

                ImGui::EndCombo();
            }

            // 슬롯이 활성화된 경우 우측에 해당 슬롯 소제목(예: "1번 정면하단") 직접 수정 가능
            if (slotEntry.enabled && !slotEntry.item.inputs.empty()) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-60);
                if (InputTextWithHintKorean("##SlotSubTitle", "슬롯 별칭 (예: 정면하단)", s_SetSlotTitleBufs[s], sizeof(s_SetSlotTitleBufs[s]))) {
                    slotEntry.item.title = s_SetSlotTitleBufs[s];
                }
                ImGui::SameLine();
                if (ImGui::Button("비움", ImVec2(48, 24))) {
                    slotEntry.enabled = false;
                    s_SetSlotTitleBufs[s][0] = '\0';
                }
            } else {
                ImGui::SameLine();
                ImGui::TextDisabled("(적용 시 인게임 슬롯 %d 비움)", s + 1);
            }

            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::Spacing();

        // 하단 저장 / 코드 복사 / 취소 버튼
        int activeCount = s_EditingSetDraft.GetActiveSlotCount();
        auto finalizeSetDraft = [&]() -> RecordingSet {
            RecordingSet result = s_EditingSetDraft;
            if (result.id.empty()) {
                result.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
            }
            result.charName = (std::strlen(s_SetCharBuf) > 0) ? s_SetCharBuf : "미지정";
            result.title = (std::strlen(s_SetTitleBuf) > 0) ? s_SetTitleBuf : (result.charName + " 녹화 세트");
            result.note = s_SetNoteBuf;

            if (result.createdAt.empty()) {
                auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::tm tmNow;
                localtime_s(&tmNow, &now);
                char timeBuf[64];
                std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", &tmNow);
                result.createdAt = timeBuf;
            }

            for (int s = 0; s < Offsets::MaxSlots; ++s) {
                if (result.slots[s].enabled && !result.slots[s].item.inputs.empty()) {
                    if (std::strlen(s_SetSlotTitleBufs[s]) > 0) {
                        result.slots[s].item.title = s_SetSlotTitleBufs[s];
                    } else if (result.slots[s].item.title.empty()) {
                        result.slots[s].item.title = "슬롯 " + std::to_string(s + 1);
                    }
                    result.slots[s].item.charName = result.charName;
                } else {
                    result.slots[s].enabled = false;
                }
            }
            return result;
        };

        ImGui::BeginDisabled(activeCount == 0);
        if (s_EditingSetIndex >= 0 && s_EditingSetIndex < (int)s_SetLibrary.size()) {
            if (ImGui::Button("세트 수정 저장 + 세트 공유 코드 복사", ImVec2(250, 36))) {
                RecordingSet updated = finalizeSetDraft();
                s_SetLibrary[s_EditingSetIndex] = updated;
                std::string shareStr = RecordingManager::ExportSetToShareString(updated);
                RecordingManager::SetClipboardText(shareStr);
                SortSetLibrary(1, true);
                RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
                ShowToast("세트가 수정되고 새 세트 공유 코드(GGSTSET1:)가 복사되었습니다!");

                ResetHangulState();
                s_EditingSetIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("세트 수정만 저장", ImVec2(150, 36))) {
                RecordingSet updated = finalizeSetDraft();
                s_SetLibrary[s_EditingSetIndex] = updated;
                SortSetLibrary(1, true);
                RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
                ShowToast("세트 보관함 항목이 수정되었습니다!");

                ResetHangulState();
                s_EditingSetIndex = -1;
                ImGui::CloseCurrentPopup();
            }
        } else {
            if (ImGui::Button("세트 보관함 저장 + 세트 코드 복사", ImVec2(240, 36))) {
                RecordingSet created = finalizeSetDraft();
                s_SetLibrary.push_back(created);
                SortSetLibrary(1, true);
                RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);

                std::string shareStr = RecordingManager::ExportSetToShareString(created);
                RecordingManager::SetClipboardText(shareStr);

                s_ShowLibrary = true;
                s_ActiveTab = 1;
                s_RequestedTab = 1;
                ShowToast("세트 보관함에 저장되고 세트 공유 코드가 복사되었습니다!");

                ResetHangulState();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("세트 보관함만 저장", ImVec2(155, 36))) {
                RecordingSet created = finalizeSetDraft();
                s_SetLibrary.push_back(created);
                SortSetLibrary(1, true);
                RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);

                s_ShowLibrary = true;
                s_ActiveTab = 1;
                s_RequestedTab = 1;
                ShowToast("세트 보관함에 저장되었습니다!");

                ResetHangulState();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("취소", ImVec2(100, 36))) {
            ResetHangulState();
            s_EditingSetIndex = -1;
            ImGui::CloseCurrentPopup();
        }

        if (activeCount == 0) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "※ 최소 1개 이상의 슬롯을 채워주세요.");
        }

        ImGui::EndPopup();
    }
}

void UI::RenderLibrary() {
    ImGui::Spacing();

    // 검색창 및 빠른 정렬 버튼 바
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "개별 녹화 검색:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240);
    InputTextWithHintKorean("##LibSearch", "캐릭터 / 제목 / 메모 검색...", s_SearchFilterBuf, sizeof(s_SearchFilterBuf));
    if (std::strlen(s_SearchFilterBuf) > 0) {
        ImGui::SameLine();
        if (ImGui::Button("초기화##LibClear", ImVec2(55, 24))) {
            ResetHangulState();
            s_SearchFilterBuf[0] = '\0';
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("캐릭터순 정렬##LibSortChar", ImVec2(105, 24))) {
        SortLibrary(1, true);
        RecordingManager::SaveRecordingLibrary(s_Library);
    }
    ImGui::SameLine();
    if (ImGui::Button("제목순 정렬##LibSortTitle", ImVec2(90, 24))) {
        SortLibrary(0, true);
        RecordingManager::SaveRecordingLibrary(s_Library);
    }

    if (s_Library.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("저장된 개별 녹화가 없습니다. 위 슬롯에서 [저장 / 공유 코드 만들기]를 누르거나 클립보드에서 붙여넣어 보세요.");
        return;
    }

    bool isPractice = GameMemory::IsPracticeMode();

    if (s_RowTargetSlots.size() < s_Library.size()) {
        s_RowTargetSlots.resize(s_Library.size(), 0);
    }

    // 남은 창 높이를 모두 활용하도록 ImVec2(0, 0) 사용 -> 창을 늘리면 표도 함께 커짐!
    ImGui::BeginChild("LibraryTableChild", ImVec2(0, 0), true);

    ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
                               | ImGuiTableFlags_Sortable
                               | ImGuiTableFlags_BordersV
                               | ImGuiTableFlags_BordersInnerH
                               | ImGuiTableFlags_RowBg
                               | ImGuiTableFlags_ScrollY
                               | ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("LibTable", 6, tableFlags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("캐릭터", ImGuiTableColumnFlags_DefaultSort, 0.13f, 1);
        ImGui::TableSetupColumn("제목",   ImGuiTableColumnFlags_None,        0.29f, 0);
        ImGui::TableSetupColumn("길이",   ImGuiTableColumnFlags_None,        0.10f, 2);
        ImGui::TableSetupColumn("메모",   ImGuiTableColumnFlags_None,        0.24f, 3);
        ImGui::TableSetupColumn("등록일", ImGuiTableColumnFlags_None,        0.10f, 4);
        ImGui::TableSetupColumn("액션 (슬롯 적용 / 코드복사 / 정보수정 / 삭제)", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 325.0f, 5);
        ImGui::TableHeadersRow();

        // 테이블 헤더 클릭 시 정렬 처리
        if (ImGuiTableSortSpecs* sortsSpecs = ImGui::TableGetSortSpecs()) {
            if (sortsSpecs->SpecsDirty && sortsSpecs->SpecsCount > 0) {
                const ImGuiTableColumnSortSpecs& spec = sortsSpecs->Specs[0];
                bool asc = (spec.SortDirection == ImGuiSortDirection_Ascending);
                SortLibrary((int)spec.ColumnUserID, asc);
                RecordingManager::SaveRecordingLibrary(s_Library);
                sortsSpecs->SpecsDirty = false;
            }
        }

        int deleteIndex = -1;
        std::string filterStr = s_SearchFilterBuf;

        for (int i = 0; i < (int)s_Library.size(); ++i) {
            const auto& item = s_Library[i];

            // 검색 필터 적용
            if (!filterStr.empty()) {
                bool match = (item.title.find(filterStr) != std::string::npos) ||
                             (item.charName.find(filterStr) != std::string::npos) ||
                             (item.note.find(filterStr) != std::string::npos);
                if (!match) continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(i);

            // 0. 캐릭터
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", item.charName.c_str());

            // 1. 제목 (줄바꿈 지원으로 잘림 방지)
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", item.title.c_str());

            // 2. 길이
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%df (%.2fs)", item.totalFrames, item.GetDurationSeconds());

            // 3. 메모 (줄바꿈 지원으로 잘림 방지)
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", item.note.c_str());

            // 4. 등록일
            ImGui::TableSetColumnIndex(4);
            ImGui::TextDisabled("%s", item.createdAt.c_str());

            // 5. 액션 버튼들
            ImGui::TableSetColumnIndex(5);

            // 드롭다운 너비를 92px로 확보하여 '슬롯 1' 글자가 화살표에 절대 가려지지 않도록 수정
            ImGui::SetNextItemWidth(92);
            const char* slotNames[] = { "슬롯 1", "슬롯 2", "슬롯 3", "슬롯 4", "슬롯 5" };
            ImGui::Combo("##ApplySlot", &s_RowTargetSlots[i], slotNames, 5);

            ImGui::SameLine(0, 5);
            ImGui::BeginDisabled(!isPractice);
            if (ImGui::Button("적용", ImVec2(44, 24))) {
                GGSTRecordingSlot slot = RecordingManager::RecordingItemToSlot(item);
                int targetSlot = s_RowTargetSlots[i];
                if (GameMemory::WriteSlot(targetSlot, slot)) {
                    ShowToast("[" + item.title + "] -> 슬롯 " + std::to_string(targetSlot + 1) + " 적용 완료!");
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0, 4);
            if (ImGui::Button("복사", ImVec2(44, 24))) {
                std::string shareStr = RecordingManager::ExportToShareString(item);
                if (RecordingManager::SetClipboardText(shareStr)) {
                    ShowToast("공유 코드가 클립보드에 복사되었습니다!");
                }
            }

            // 저장된 녹화 정보(캐릭터/제목/메모) 수정 버튼
            ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.45f, 0.68f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.56f, 0.82f, 1.00f));
            if (ImGui::Button("정보수정", ImVec2(66, 24))) {
                s_EditingLibraryIndex = i;
                s_ExportSourceSlot = -1;
                strncpy_s(s_ExportCharBuf, item.charName.c_str(), _TRUNCATE);
                strncpy_s(s_ExportTitleBuf, item.title.c_str(), _TRUNCATE);
                strncpy_s(s_ExportNoteBuf, item.note.c_str(), _TRUNCATE);
                ResetHangulState();
                s_ShowExportModal = true;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine(0, 4);
            if (ImGui::Button("삭제", ImVec2(44, 24))) {
                deleteIndex = i;
            }

            ImGui::PopID();
        }

        if (deleteIndex >= 0 && deleteIndex < (int)s_Library.size()) {
            s_Library.erase(s_Library.begin() + deleteIndex);
            if (deleteIndex < (int)s_RowTargetSlots.size()) {
                s_RowTargetSlots.erase(s_RowTargetSlots.begin() + deleteIndex);
            }
            RecordingManager::SaveRecordingLibrary(s_Library);
            ShowToast("항목이 삭제되었습니다.");
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void UI::RenderSetLibrary() {
    ImGui::Spacing();

    // 상단 검색창 + 정렬 + [새 세트 만들기] 버튼 바
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "세트 검색:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(220);
    InputTextWithHintKorean("##SetSearch", "캐릭터 / 세트 제목 / 메모 검색...", s_SetSearchFilterBuf, sizeof(s_SetSearchFilterBuf));
    if (std::strlen(s_SetSearchFilterBuf) > 0) {
        ImGui::SameLine();
        if (ImGui::Button("초기화##SetClear", ImVec2(55, 24))) {
            ResetHangulState();
            s_SetSearchFilterBuf[0] = '\0';
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("캐릭터순 정렬##SetSortChar", ImVec2(105, 24))) {
        SortSetLibrary(1, true);
        RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
    }
    ImGui::SameLine();
    if (ImGui::Button("제목순 정렬##SetSortTitle", ImVec2(90, 24))) {
        SortSetLibrary(0, true);
        RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
    }

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.35f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.68f, 0.44f, 1.00f));
    if (ImGui::Button("보관함 조합으로 새 세트 만들기", ImVec2(210, 24))) {
        s_EditingSetIndex = -1;
        s_EditingSetDraft = RecordingSet{};
        s_SetTitleBuf[0] = '\0';
        s_SetNoteBuf[0] = '\0';
        for (int s = 0; s < Offsets::MaxSlots; ++s) {
            s_SetSlotTitleBufs[s][0] = '\0';
        }
        ResetHangulState();
        s_ShowSetModal = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.28f, 0.68f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.36f, 0.82f, 1.00f));
    if (ImGui::Button("현재 슬롯 1~5 묶어서 세트 만들기##SetTabBtn", ImVec2(225, 24))) {
        OpenSetModalFromCurrentGameSlots();
    }
    ImGui::PopStyleColor(2);

    if (s_SetLibrary.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("저장된 녹화 세트가 없습니다. [보관함 조합으로 새 세트 만들기] 또는 [현재 슬롯 1~5 묶어서 세트 만들기]를 눌러 세트를 만들어 보세요!");
        return;
    }

    bool isPractice = GameMemory::IsPracticeMode();

    ImGui::BeginChild("SetLibraryTableChild", ImVec2(0, 0), true);

    ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
                               | ImGuiTableFlags_Sortable
                               | ImGuiTableFlags_BordersV
                               | ImGuiTableFlags_BordersInnerH
                               | ImGuiTableFlags_RowBg
                               | ImGuiTableFlags_ScrollY
                               | ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("SetLibTable", 6, tableFlags)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("캐릭터",            ImGuiTableColumnFlags_DefaultSort, 0.11f, 1);
        ImGui::TableSetupColumn("세트 제목",         ImGuiTableColumnFlags_None,        0.24f, 0);
        ImGui::TableSetupColumn("슬롯 1~5 구성",     ImGuiTableColumnFlags_None,        0.31f, 2);
        ImGui::TableSetupColumn("메모 / 조건",       ImGuiTableColumnFlags_None,        0.18f, 3);
        ImGui::TableSetupColumn("등록일",            ImGuiTableColumnFlags_None,        0.09f, 4);
        ImGui::TableSetupColumn("세트 액션 (슬롯 1~5 일괄 교체)", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed, 305.0f, 5);
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sortsSpecs = ImGui::TableGetSortSpecs()) {
            if (sortsSpecs->SpecsDirty && sortsSpecs->SpecsCount > 0) {
                const ImGuiTableColumnSortSpecs& spec = sortsSpecs->Specs[0];
                bool asc = (spec.SortDirection == ImGuiSortDirection_Ascending);
                SortSetLibrary((int)spec.ColumnUserID, asc);
                RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
                sortsSpecs->SpecsDirty = false;
            }
        }

        int deleteIndex = -1;
        std::string filterStr = s_SetSearchFilterBuf;

        for (int i = 0; i < (int)s_SetLibrary.size(); ++i) {
            const auto& setItem = s_SetLibrary[i];
            std::string summaryStr = setItem.GetSlotSummaryText();

            if (!filterStr.empty()) {
                bool match = (setItem.title.find(filterStr) != std::string::npos) ||
                             (setItem.charName.find(filterStr) != std::string::npos) ||
                             (setItem.note.find(filterStr) != std::string::npos) ||
                             (summaryStr.find(filterStr) != std::string::npos);
                if (!match) continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(i);

            // 0. 캐릭터
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", setItem.charName.c_str());

            // 1. 세트 제목
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", setItem.title.c_str());

            // 2. 슬롯 1~5 요약
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(ImVec4(0.65f, 0.95f, 0.70f, 1.0f), "[%d개 슬롯]", setItem.GetActiveSlotCount());
            ImGui::SameLine();
            ImGui::TextWrapped("%s", summaryStr.c_str());

            // 3. 메모
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", setItem.note.c_str());

            // 4. 등록일
            ImGui::TableSetColumnIndex(4);
            ImGui::TextDisabled("%s", setItem.createdAt.c_str());

            // 5. 액션 버튼들
            ImGui::TableSetColumnIndex(5);

            ImGui::BeginDisabled(!isPractice);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.32f, 0.15f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.42f, 0.20f, 1.00f));
            if (ImGui::Button("슬롯 일괄적용", ImVec2(110, 24))) {
                if (ApplySetToGameSlots(setItem)) {
                    ShowToast("세트 [" + setItem.title + "] -> 슬롯 1~5 일괄 적용 완료! (빈 슬롯은 비워짐)");
                } else {
                    ShowToast("세트 일괄 적용 중 오류가 발생했습니다.");
                }
            }
            ImGui::PopStyleColor(2);
            ImGui::EndDisabled();

            ImGui::SameLine(0, 4);
            if (ImGui::Button("복사", ImVec2(48, 24))) {
                std::string shareStr = RecordingManager::ExportSetToShareString(setItem);
                if (RecordingManager::SetClipboardText(shareStr)) {
                    ShowToast("세트 공유 코드(GGSTSET1:)가 클립보드에 복사되었습니다!");
                }
            }

            ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.45f, 0.68f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.56f, 0.82f, 1.00f));
            if (ImGui::Button("정보수정", ImVec2(66, 24))) {
                s_EditingSetIndex = i;
                s_EditingSetDraft = setItem;
                strncpy_s(s_SetCharBuf, setItem.charName.c_str(), _TRUNCATE);
                strncpy_s(s_SetTitleBuf, setItem.title.c_str(), _TRUNCATE);
                strncpy_s(s_SetNoteBuf, setItem.note.c_str(), _TRUNCATE);
                for (int s = 0; s < Offsets::MaxSlots; ++s) {
                    if (setItem.slots[s].enabled) {
                        strncpy_s(s_SetSlotTitleBufs[s], setItem.slots[s].item.title.c_str(), _TRUNCATE);
                    } else {
                        s_SetSlotTitleBufs[s][0] = '\0';
                    }
                }
                ResetHangulState();
                s_ShowSetModal = true;
            }
            ImGui::PopStyleColor(2);

            ImGui::SameLine(0, 4);
            if (ImGui::Button("삭제", ImVec2(48, 24))) {
                deleteIndex = i;
            }

            ImGui::PopID();
        }

        if (deleteIndex >= 0 && deleteIndex < (int)s_SetLibrary.size()) {
            s_SetLibrary.erase(s_SetLibrary.begin() + deleteIndex);
            RecordingManager::SaveRecordingSetLibrary(s_SetLibrary);
            ShowToast("세트가 삭제되었습니다.");
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

void UI::RenderBottomSection() {
    if (!s_ShowLibrary) return;

    ImGui::Spacing();
    if (ImGui::BeginTabBar("LibraryAndSetTabs", ImGuiTabBarFlags_None)) {
        ImGuiTabItemFlags tab0Flags = (s_RequestedTab == 0) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        ImGuiTabItemFlags tab1Flags = (s_RequestedTab == 1) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        s_RequestedTab = -1;

        std::string tab0Title = "로컬 개별 녹화 보관함 (" + std::to_string(s_Library.size()) + ")###TabIndividual";
        if (ImGui::BeginTabItem(tab0Title.c_str(), NULL, tab0Flags)) {
            s_ActiveTab = 0;
            RenderLibrary();
            ImGui::EndTabItem();
        }

        std::string tab1Title = "로컬 세트 보관함 - 슬롯 1~5 일괄 교체 (" + std::to_string(s_SetLibrary.size()) + ")###TabSets";
        if (ImGui::BeginTabItem(tab1Title.c_str(), NULL, tab1Flags)) {
            s_ActiveTab = 1;
            RenderSetLibrary();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void UI::Render() {
    // 창 최소/최대 크기 제한 설정 및 자유로운 마우스 리사이즈 허용
    ImGui::SetNextWindowSizeConstraints(ImVec2(860, 520), ImVec2(1920, 1080));
    ImGui::SetNextWindowSize(ImVec2(1080, 740), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("GGST 프랙티스 녹화 공유기 (만든이 : 남양주빌런) - 제미나이4 아르곤 활용 제작###MainWindow", NULL,
                     ImGuiWindowFlags_NoCollapse)) {
        RenderHeader();
        RenderSlots();
        RenderBottomSection();

        RenderConfirmModal();
        RenderConfirmSetModal();
        RenderExportModal();
        RenderSetModal();
    }
    ImGui::End();
}

} // namespace GGST
