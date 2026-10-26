#include <windows.h>
#include "game/game_memory.h"
#include "core/config.h"
#include "core/dx11_hook.h"

static HMODULE g_hModule = NULL;

DWORD WINAPI ModInitThread(LPVOID lpParam) {
    // 1. 설정 로드
    GGST::Config::Load();

    // 2. SteamStub 3.1 복호화 대기 (게임 로딩 중 크래시 방지)
    // .text RVA 0x1000 == 0xB8 확인
    GGST::GameMemory::WaitForDecryption(30000);

    // 3. 게임 메모리 모듈 초기화
    GGST::GameMemory::Initialize();

    // 4. DX11 Present 후킹 및 ImGui 초기화
    GGST::DX11Hook::Initialize();

    return 0;
}

BOOL WINAPI DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        GGST::Config::Initialize(hModule);
        CreateThread(NULL, 0, ModInitThread, NULL, 0, NULL);
        break;

    case DLL_PROCESS_DETACH:
        if (!lpReserved) {
            GGST::DX11Hook::Shutdown();
        }
        break;
    }
    return TRUE;
}

// UE4SS C++ Mod 연동 인터페이스 (UE4SS가 main.dll 로드 시 호출하여 RC::CppUserModBase* 반환 기대)
static void __fastcall DummyVirtualMethod(void* /*self*/) {}

static void* s_FallbackVTable[32] = {
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod,
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod,
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod,
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod,
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod,
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod,
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod,
    (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod, (void*)&DummyVirtualMethod
};

extern "C" __declspec(dllexport) void* start_mod() {
    // RC::CppUserModBase 크기(0xE8)보다 넉넉한 0x200 바이트 할당 및 0 초기화
    uint8_t* modInstance = new uint8_t[0x200]();

    HMODULE hUE4SS = GetModuleHandleW(L"UE4SS.dll");
    if (hUE4SS) {
        using CppUserModBaseCtorFn = void(__fastcall*)(void*);
        auto ctor = reinterpret_cast<CppUserModBaseCtorFn>(
            GetProcAddress(hUE4SS, "??0CppUserModBase@RC@@QEAA@XZ"));
        if (ctor) {
            ctor(modInstance);
            return modInstance;
        }
    }

    // 폴백: 가상 함수 테이블 포인터를 안전한 No-Op 테이블로 설정
    *reinterpret_cast<void***>(modInstance) = s_FallbackVTable;
    return modInstance;
}

extern "C" __declspec(dllexport) void uninstall_mod(void* mod) {
    GGST::DX11Hook::Shutdown();
    if (mod) {
        HMODULE hUE4SS = GetModuleHandleW(L"UE4SS.dll");
        if (hUE4SS) {
            using CppUserModBaseDtorFn = void(__fastcall*)(void*);
            auto dtor = reinterpret_cast<CppUserModBaseDtorFn>(
                GetProcAddress(hUE4SS, "??1CppUserModBase@RC@@UEAA@XZ"));
            if (dtor) {
                dtor(mod);
            }
        }
        delete[] reinterpret_cast<uint8_t*>(mod);
    }
}
