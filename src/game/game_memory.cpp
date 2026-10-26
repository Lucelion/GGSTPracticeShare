#include "game_memory.h"
#include <cstring>

namespace GGST {

uintptr_t GameMemory::s_ImageBase = 0;
uintptr_t GameMemory::s_GWorldRVA = Offsets::GWorld;
uintptr_t GameMemory::s_GGameCommonRVA = Offsets::GGameCommon;
uintptr_t GameMemory::s_GSaveDataManagerRVA = Offsets::GSaveDataManager;
bool GameMemory::s_IsInitialized = false;
static bool s_SignaturesScanned = false;
static int s_ScanAttempts = 0;

static uintptr_t FindBytes(const uint8_t* base, size_t size, const uint8_t* pattern, const char* mask) {
    size_t patLen = std::strlen(mask);
    if (size < patLen) return 0;
    for (size_t i = 0; i <= size - patLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < patLen; ++j) {
            if (mask[j] == 'x' && base[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }
    return 0;
}

bool GameMemory::Initialize() {
    s_ImageBase = (uintptr_t)GetModuleHandleW(L"GGST-Win64-Shipping.exe");
    if (!s_ImageBase) {
        s_ImageBase = (uintptr_t)GetModuleHandleW(NULL);
    }
    if (!s_ImageBase) return false;

    // .text 전체 복호화 완료 후 AOB 시그니처 스캔으로 RVA 동적 검증 (실패 시 실측 기본 RVA 사용)
    if (!s_SignaturesScanned && s_ScanAttempts < 5) {
        __try {
            const uint8_t* textBase = reinterpret_cast<const uint8_t*>(s_ImageBase + 0x1000);
            constexpr size_t textSize = 0x3668000;
            if (textBase[0] == 0xB8) {
                s_ScanAttempts++;
                bool foundGW = false, foundSD = false, foundGC = false;

                // 1. GWorld: 0F 2E C1 74 51 48 8B 1D ?? ?? ?? ?? 48 85 DB 74
                const uint8_t patGW[] = { 0x0F, 0x2E, 0xC1, 0x74, 0x51, 0x48, 0x8B, 0x1D, 0, 0, 0, 0, 0x48, 0x85, 0xDB, 0x74 };
                uintptr_t offGW = FindBytes(textBase, textSize, patGW, "xxxxxxxx????xxxx");
                if (offGW) {
                    int32_t disp = *reinterpret_cast<const int32_t*>(textBase + offGW + 8);
                    s_GWorldRVA = 0x1000 + offGW + 5 + 7 + disp;
                    foundGW = true;
                }

                // 2. GSaveDataManager: 48 63 43 04 48 69 C8 28 15 00 00 48 8B 05 ?? ?? ?? ?? 48 05 D4 84 02 00
                const uint8_t patSD[] = { 0x48, 0x63, 0x43, 0x04, 0x48, 0x69, 0xC8, 0x28, 0x15, 0x00, 0x00, 0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x48, 0x05, 0xD4, 0x84, 0x02, 00 };
                uintptr_t offSD = FindBytes(textBase, textSize, patSD, "xxxxxxxxxxxxxx????xxxxxx");
                if (offSD) {
                    int32_t disp = *reinterpret_cast<const int32_t*>(textBase + offSD + 14);
                    s_GSaveDataManagerRVA = 0x1000 + offSD + 11 + 7 + disp;
                    foundSD = true;
                }

                // 3. GGameCommon: 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 F8 17 0F 85
                const uint8_t patGC[] = { 0x48, 0x8B, 0x0D, 0, 0, 0, 0, 0xE8, 0, 0, 0, 0, 0x83, 0xF8, 0x17, 0x0F, 0x85 };
                uintptr_t offGC = FindBytes(textBase, textSize, patGC, "xxx????x????xxxxx");
                if (offGC) {
                    int32_t disp = *reinterpret_cast<const int32_t*>(textBase + offGC + 3);
                    s_GGameCommonRVA = 0x1000 + offGC + 7 + disp;
                    foundGC = true;
                }

                if (foundGW && foundSD && foundGC) {
                    s_SignaturesScanned = true;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    s_IsInitialized = true;
    return true;
}

bool GameMemory::WaitForDecryption(DWORD timeoutMs) {
    if (!s_ImageBase) {
        s_ImageBase = (uintptr_t)GetModuleHandleW(L"GGST-Win64-Shipping.exe");
        if (!s_ImageBase) s_ImageBase = (uintptr_t)GetModuleHandleW(NULL);
        if (!s_ImageBase) return false;
    }

    DWORD start = GetTickCount();
    while (GetTickCount() - start < timeoutMs) {
        __try {
            // SteamStub 3.1 복호화 확인: .text RVA 0x1000이 복호화되면 첫 바이트가 0xB8
            // 단, 57MB .text 영역 전체가 끝까지 복호화될 때까지 200ms 추가 대기
            uint8_t b = *(uint8_t*)(s_ImageBase + 0x1000);
            if (b == 0xB8) {
                Sleep(250);
                return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // 아직 메모리 접근 불가일 수 있음
        }
        Sleep(50);
    }
    return false;
}

bool GameMemory::IsReady() {
    if (!s_IsInitialized || (!s_SignaturesScanned && s_ScanAttempts < 5)) {
        Initialize();
    }
    return s_IsInitialized;
}

uint8_t GameMemory::GetGameMode() {
    if (!IsReady()) return 0xFF;

    __try {
        uintptr_t pGameCommon = *(uintptr_t*)(s_ImageBase + s_GGameCommonRVA);
        if (!pGameCommon) return 0xFF;
        return *(uint8_t*)(pGameCommon + Offsets::GameModeOffset);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0xFF;
    }
}

uint8_t GameMemory::GetMainPlayerSide() {
    if (!IsReady()) return 0;

    __try {
        uintptr_t pGameCommon = *(uintptr_t*)(s_ImageBase + s_GGameCommonRVA);
        if (!pGameCommon) return 0;
        return *(uint8_t*)(pGameCommon + Offsets::MainSideOffset);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool GameMemory::IsPracticeMode() {
    if (!IsReady()) return false;

    __try {
        // 1. GameCommon 및 GameMode 검사
        uintptr_t pGameCommon = *(uintptr_t*)(s_ImageBase + s_GGameCommonRVA);
        if (!pGameCommon) return false;

        uint8_t gameMode = *(uint8_t*)(pGameCommon + Offsets::GameModeOffset);
        // 8 = 오프라인 프랙티스(Training Mode), 19 = 트레이닝 대기 모드
        if (gameMode != 8 && gameMode != 19) {
            return false;
        }

        // 2. GWorld 및 GameState 검사 (인게임 매치 중인지)
        uintptr_t pGWorld = *(uintptr_t*)(s_ImageBase + s_GWorldRVA);
        if (!pGWorld) return false;

        uintptr_t pGameState = *(uintptr_t*)(pGWorld + Offsets::WorldToGameState);
        if (!pGameState) return false;

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uintptr_t GameMemory::GetSlotAddress(int slotIndex) {
    if (!IsReady()) return 0;
    if (slotIndex < 0 || slotIndex >= Offsets::MaxSlots) return 0;

    __try {
        uintptr_t pSaveData = *(uintptr_t*)(s_ImageBase + s_GSaveDataManagerRVA);
        if (!pSaveData) return 0;

        return pSaveData + Offsets::SlotBaseOffset + (slotIndex * Offsets::SlotStride);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

bool GameMemory::ReadSlot(int slotIndex, GGSTRecordingSlot& outSlot) {
    // 안전 원칙: 프랙티스 모드일 때만 슬롯 접근 허용
    if (!IsPracticeMode()) return false;

    uintptr_t slotAddr = GetSlotAddress(slotIndex);
    if (!slotAddr) return false;

    __try {
        const auto* src = reinterpret_cast<const GGSTRecordingSlot*>(slotAddr);
        std::memcpy(&outSlot, src, sizeof(GGSTRecordingSlot));
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool GameMemory::WriteSlot(int slotIndex, const GGSTRecordingSlot& inSlot) {
    // 안전 원칙: 프랙티스 모드일 때만 슬롯 접근 허용 (온라인 악용 원천 차단)
    if (!IsPracticeMode()) return false;

    uintptr_t slotAddr = GetSlotAddress(slotIndex);
    if (!slotAddr) return false;

    __try {
        DWORD oldProtect = 0;
        if (VirtualProtect(reinterpret_cast<void*>(slotAddr), sizeof(GGSTRecordingSlot), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            auto* dst = reinterpret_cast<GGSTRecordingSlot*>(slotAddr);
            std::memcpy(dst, &inSlot, sizeof(GGSTRecordingSlot));
            VirtualProtect(reinterpret_cast<void*>(slotAddr), sizeof(GGSTRecordingSlot), oldProtect, &oldProtect);
            return true;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return false;
}

bool GameMemory::ClearSlot(int slotIndex) {
    if (!IsPracticeMode()) return false;

    GGSTRecordingSlot emptySlot{};
    emptySlot.bHasData = 0;
    emptySlot.TotalFrameCount = -1;
    emptySlot.StartOffsetFrame = 0;
    emptySlot.RecordedFacingDir = 0;

    return WriteSlot(slotIndex, emptySlot);
}

bool GameMemory::GetBattlePlayerInfo(uint8_t& outP1Char, int32_t& outP1Facing,
                                     uint8_t& outP2Char, int32_t& outP2Facing) {
    if (!IsPracticeMode()) return false;

    __try {
        uintptr_t pGWorld = *(uintptr_t*)(s_ImageBase + s_GWorldRVA);
        if (!pGWorld) return false;

        uintptr_t pGameState = *(uintptr_t*)(pGWorld + Offsets::WorldToGameState);
        if (!pGameState) return false;

        uintptr_t pBattleObjMgr = *(uintptr_t*)(pGameState + Offsets::GameStateToEngine);
        if (!pBattleObjMgr) return false;

        outP1Char   = *(uint8_t*)(pBattleObjMgr + Offsets::P1_CharId);
        outP1Facing = *(int32_t*)(pBattleObjMgr + Offsets::P1_FacingDir);

        outP2Char   = *(uint8_t*)(pBattleObjMgr + Offsets::P2_CharId);
        outP2Facing = *(int32_t*)(pBattleObjMgr + Offsets::P2_FacingDir);

        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace GGST
