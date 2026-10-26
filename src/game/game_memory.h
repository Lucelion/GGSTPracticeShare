#pragma once

#include "ggst_types.h"
#include <windows.h>

namespace GGST {

class GameMemory {
public:
    static bool Initialize();
    static bool WaitForDecryption(DWORD timeoutMs = 20000);
    static bool IsReady();

    // 프랙티스 모드 판별 (온라인 악용 차단 Hard-Lock)
    static bool IsPracticeMode();
    static uint8_t GetGameMode();
    static uint8_t GetMainPlayerSide(); // 0 = 1P, 1 = 2P

    // 슬롯 메모리 제어
    static uintptr_t GetSlotAddress(int slotIndex);
    static bool ReadSlot(int slotIndex, GGSTRecordingSlot& outSlot);
    static bool WriteSlot(int slotIndex, const GGSTRecordingSlot& inSlot);
    static bool ClearSlot(int slotIndex);

    // 현재 배틀 캐릭터 상태
    static bool GetBattlePlayerInfo(uint8_t& outP1Char, int32_t& outP1Facing,
                                    uint8_t& outP2Char, int32_t& outP2Facing);

    static uintptr_t GetImageBase() { return s_ImageBase; }

private:
    static uintptr_t s_ImageBase;
    static uintptr_t s_GWorldRVA;
    static uintptr_t s_GGameCommonRVA;
    static uintptr_t s_GSaveDataManagerRVA;
    static bool s_IsInitialized;
};

} // namespace GGST
