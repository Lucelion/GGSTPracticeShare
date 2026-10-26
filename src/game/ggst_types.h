#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace GGST {

#pragma pack(push, 1)
// 프랙티스 내장 녹화 슬롯 구조체 (sizeof == 0x1528 = 5,416 Bytes)
// RVA 0x11D6D30, 0xBD5F61, 0xBD1165 검증 완료
struct GGSTRecordingSlot {
    uint8_t  bHasData;            // +0x00: 데이터가 있으면 1, 비어있으면 0
    uint8_t  Pad01[3];            // +0x01: 패딩 (0)
    int32_t  RecordedFacingDir;   // +0x04: 녹화 시점 더미 시선 방향 (0 = 오른쪽/1P방향, 1 = 왼쪽/2P방향)
    int32_t  TotalFrameCount;     // +0x08: 마지막 녹화 프레임 인덱스 (비어있을 때 -1 / 0xFFFFFFFF, 유효할 때 > 4)
    int32_t  StartOffsetFrame;    // +0x0C: 시작 오프셋 프레임 (기본 0, 실제 재생 길이 = TotalFrameCount - StartOffsetFrame)
    uint16_t FrameInputs[2700];   // +0x10 ~ +0x1527: 최대 2700프레임 (45.0초 @ 60fps) 비트마스크 (0x1518 바이트)
};
#pragma pack(pop)
static_assert(sizeof(GGSTRecordingSlot) == 0x1528, "GGSTRecordingSlot size must be 0x1528 bytes");

// 핵심 RVA 오프셋 정의 (GGST-Win64-Shipping.exe 최신 빌드 실측 기준)
namespace Offsets {
    constexpr uintptr_t GWorld            = 0x529B2E0;
    constexpr uintptr_t GGameCommon       = 0x50083E0;
    constexpr uintptr_t GSaveDataManager  = 0x4FE7800;
    constexpr uintptr_t GTrainingManager  = 0x4AD32E0;

    // 슬롯 메모리 오프셋
    constexpr uintptr_t SlotBaseOffset    = 0x284D4;
    constexpr uintptr_t SlotStride        = 0x1528;
    constexpr int       MaxSlots          = 5;
    constexpr int       MaxFrames         = 2700;

    // GameCommon 필드
    constexpr uintptr_t GameModeOffset    = 0x2F0; // 8 = 오프라인 프랙티스, 19 = 트레이닝 대기
    constexpr uintptr_t MainSideOffset    = 0x2F4; // 0 = 1P, 1 = 2P

    // UWorld -> GameState -> BattleObjMgr
    constexpr uintptr_t WorldToGameState  = 0x130;
    constexpr uintptr_t GameStateToEngine = 0xC20; // asw_engine / battleObjMgr

    // 배틀 오브젝트 오프셋
    constexpr uintptr_t P1_CharId         = 0x274DDE;
    constexpr uintptr_t P1_FacingDir      = 0x27B0AC; // 0 = 오른쪽, 1 = 왼쪽
    constexpr uintptr_t PlayerStride      = 0xADB80;
    constexpr uintptr_t P2_CharId         = P1_CharId + PlayerStride;     // 0x32295E
    constexpr uintptr_t P2_FacingDir      = P1_FacingDir + PlayerStride;  // 0x328C2C
}

// 캐릭터 정보 구조체
struct CharacterInfo {
    uint8_t id;
    const char* code;
    const char* nameKo;
    const char* nameEn;
};

// 32명 캐릭터 정보 테이블 (0 ~ 31)
inline const CharacterInfo CharacterTable[] = {
    {  0, "SOL", "솔 배드가이", "Sol Badguy" },
    {  1, "KYK", "카이 키스크", "Ky Kiske" },
    {  2, "MAY", "메이", "May" },
    {  3, "AXL", "액슬 로우", "Axl Low" },
    {  4, "CHP", "치프 자너프", "Chipp Zanuff" },
    {  5, "POT", "포템킨", "Potemkin" },
    {  6, "FAU", "파우스트", "Faust" },
    {  7, "MLL", "밀리아 레이지", "Millia Rage" },
    {  8, "ZAT", "자토-ONE", "Zato-1" },
    {  9, "RAM", "램리썰 밸런타인", "Ramlethal Valentine" },
    { 10, "LEO", "레오 화이트팽", "Leo Whitefang" },
    { 11, "NAG", "나고리유키", "Nagoriyuki" },
    { 12, "GIO", "지오바나", "Giovanna" },
    { 13, "ANJ", "미토 안지", "Anji Mito" },
    { 14, "INO", "이노", "I-No" },
    { 15, "GLD", "골드루이스 디킨슨", "Goldlewis Dickinson" },
    { 16, "JKO", "잭오", "Jack-O'" },
    { 17, "COS", "해피 케이오스", "Happy Chaos" },
    { 18, "BKN", "바이켄", "Baiken" },
    { 19, "TST", "테스타먼트", "Testament" },
    { 20, "BGT", "브리짓", "Bridget" },
    { 21, "SIN", "신 키스크", "Sin Kiske" },
    { 22, "BED", "베드맨?", "Bedman?" },
    { 23, "ASK", "아스카 R#", "Asuka R#" },
    { 24, "JHN", "조니", "Johnny" },
    { 25, "ELP", "엘페르트 밸런타인", "Elphelt Valentine" },
    { 26, "ABA", "아바", "A.B.A" },
    { 27, "SLY", "슬레이어", "Slayer" },
    { 28, "DZY", "퀸 디지", "Queen Dizzy" },
    { 29, "VEN", "베놈", "Venom" },
    { 30, "UNI", "유니카", "Unika" },
    { 31, "LUC", "루시", "Lucy" },
};

inline const CharacterInfo* FindCharacter(uint8_t id) {
    if (id < sizeof(CharacterTable) / sizeof(CharacterTable[0])) {
        return &CharacterTable[id];
    }
    return nullptr;
}

inline std::string FormatCharacterName(uint8_t id) {
    const CharacterInfo* info = FindCharacter(id);
    if (info) {
        return std::string(info->nameKo) + " (" + info->nameEn + ")";
    }
    return "알 수 없는 캐릭터 (" + std::to_string(id) + ")";
}

} // namespace GGST
