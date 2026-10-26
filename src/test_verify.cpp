#include <iostream>
#include <cassert>
#include "core/recording_data.h"
#include "core/config.h"

int main() {
    std::cout << "[TEST] 1. 단축키 이름 변환 테스트..." << std::endl;
    assert(GGST::Config::GetKeyDisplayName(VK_F12) == "F12");
    assert(GGST::Config::GetKeyDisplayName(VK_F8) == "F8");
    assert(GGST::Config::GetKeyDisplayName(VK_INSERT) == "INSERT");
    assert(GGST::Config::GetKeyDisplayName(VK_ESCAPE) == "ESC");
    std::cout << "  -> 단축키 이름 변환 성공!" << std::endl;

    std::cout << "[TEST] 2. 공유 문자열 압축/인코딩/파싱 왕복(Round-trip) 테스트..." << std::endl;
    GGST::RecordingItem original;
    original.id = "test_id_123";
    original.title = "솔 근S 가드 후 정역 이지 1";
    original.charId = 0;
    original.charName = "솔 배드가이 (Sol Badguy)";
    original.totalFrames = 145;
    original.facingDir = 0;
    original.note = "구석 시작 권장";
    original.createdAt = "2026-10-26 19:00";
    original.inputs.resize(145);
    for (int i = 0; i < 145; ++i) {
        original.inputs[i] = (uint16_t)(i * 3 + 1);
    }

    std::string shareStr = GGST::RecordingManager::ExportToShareString(original);
    std::cout << "  -> 생성된 공유 코드 (" << shareStr.size() << " chars): " << shareStr << std::endl;
    assert(shareStr.rfind("GGSTREC1:", 0) == 0);

    GGST::RecordingItem parsed;
    std::string err;
    bool success = GGST::RecordingManager::ImportFromShareString(shareStr, parsed, err);
    if (!success) {
        std::cerr << "  [FAIL] 파싱 실패: " << err << std::endl;
        return 1;
    }

    assert(parsed.title == original.title);
    assert(parsed.charId == original.charId);
    assert(parsed.totalFrames == original.totalFrames);
    assert(parsed.facingDir == original.facingDir);
    assert(parsed.note == original.note);
    assert(parsed.inputs.size() == original.inputs.size());
    for (size_t i = 0; i < original.inputs.size(); ++i) {
        assert(parsed.inputs[i] == original.inputs[i]);
    }
    std::cout << "  -> 복원된 데이터 100% 일치 확인!" << std::endl;

    std::cout << "[TEST] 3. GGST 슬롯 변환 상호 호환성 검증..." << std::endl;
    GGST::GGSTRecordingSlot slot = GGST::RecordingManager::RecordingItemToSlot(parsed);
    assert(slot.bHasData == 1);
    assert(slot.TotalFrameCount == 145);
    assert(slot.RecordedFacingDir == 0);
    assert(slot.FrameInputs[0] == parsed.inputs[0]);
    assert(slot.FrameInputs[144] == parsed.inputs[144]);
    assert(slot.FrameInputs[145] == 0); // 미사용 영역 0 초기화 확인
    std::cout << "  -> GGSTRecordingSlot 구조체 주입 데이터 일치 확인!" << std::endl;

    std::cout << "\n==========================================" << std::endl;
    std::cout << " 모든 테스트 통과 (All Tests Passed)!" << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
