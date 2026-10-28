/*
 * dinput8.dll proxy for Metro 2033 Redux (next-gen build) on CrossOver/D3DMetal.
 *
 * The game's FSR setup runs even when its D3D11->D3D12 bridge failed and
 * dereferences a NULL D3D12 device. Widening the run-once check from
 * `cmp byte` to `cmp dword` (0x80 -> 0x83) makes it also see the adjacent
 * bridge-failed flag, so FSR setup is skipped. metro.exe is SteamStub
 * encrypted on disk, so the byte is flipped in memory once the stub has
 * decrypted it. Only patches if all 12 expected bytes match.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define PATCH_RVA 0x831E1F

static const unsigned char EXPECTED[] = {
    0x80, 0x3D, 0xF5, 0x31, 0x7A, 0x00, 0x00, 0x48, 0x8B, 0xD9, 0x0F, 0x85,
};

typedef HRESULT(WINAPI *DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
static DirectInput8Create_t real_DirectInput8Create;

static void log_line(const char *msg)
{
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    while (n && path[n - 1] != '\\') n--;
    strcpy(path + n, "metro2033_fsr_fix.log");
    FILE *f = fopen(path, "a");
    if (!f) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(f, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, msg);
    fclose(f);
}

static DWORD WINAPI patch_thread(LPVOID unused)
{
    unsigned char *target = (unsigned char *)GetModuleHandleA(NULL) + PATCH_RVA;
    DWORD start = GetTickCount();

    while (GetTickCount() - start < 30000) {
        if (target[0] == 0x83 && memcmp(target + 1, EXPECTED + 1, sizeof(EXPECTED) - 1) == 0) {
            log_line("already patched");
            return 0;
        }
        if (memcmp(target, EXPECTED, sizeof(EXPECTED)) == 0) {
            DWORD old;
            VirtualProtect(target, 1, PAGE_EXECUTE_READWRITE, &old);
            target[0] = 0x83;
            VirtualProtect(target, 1, old, &old);
            FlushInstructionCache(GetCurrentProcess(), target, 1);
            char msg[64];
            sprintf(msg, "patched after %lu ms", GetTickCount() - start);
            log_line(msg);
            return 0;
        }
        Sleep(1);
    }
    log_line("expected bytes never appeared (different build?) - not patching");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        CloseHandle(CreateThread(NULL, 0, patch_thread, NULL, 0, NULL));
    }
    return TRUE;
}

__declspec(dllexport) HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID riid, LPVOID *out, LPUNKNOWN outer)
{
    if (!real_DirectInput8Create) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        strcat(path, "\\dinput8.dll");
        HMODULE real = LoadLibraryA(path);
        if (real) real_DirectInput8Create = (DirectInput8Create_t)GetProcAddress(real, "DirectInput8Create");
    }
    if (!real_DirectInput8Create) return E_FAIL;
    return real_DirectInput8Create(inst, ver, riid, out, outer);
}
