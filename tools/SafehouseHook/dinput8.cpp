#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include "MinHook.h"

#pragma comment(lib, "MinHook.lib")

//--------------------------------------------------------------------------
// XInput dynamic binding
//--------------------------------------------------------------------------
typedef struct _XINPUT_GAMEPAD {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD;

typedef struct _XINPUT_STATE {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;

typedef DWORD (WINAPI* PFN_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
static PFN_XInputGetState g_pfnXInputGetState = NULL;

#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000

//--------------------------------------------------------------------------
// dinput8 Proxy Export
//--------------------------------------------------------------------------
typedef HRESULT(WINAPI* PFN_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
static PFN_DirectInput8Create g_oDirectInput8Create = NULL;

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    if (!g_oDirectInput8Create)
    {
        char sysPath[MAX_PATH];
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\dinput8.dll");
        HMODULE hOrig = LoadLibraryA(sysPath);
        if (hOrig)
        {
            g_oDirectInput8Create = (PFN_DirectInput8Create)GetProcAddress(hOrig, "DirectInput8Create");
        }
    }
    if (g_oDirectInput8Create)
    {
        return g_oDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
    return E_FAIL;
}

//--------------------------------------------------------------------------
// Safehouse Definitions
//--------------------------------------------------------------------------
struct SafehouseDoor {
    const char* name;
    float ext_x, ext_y, ext_z;
    float int_x, int_y, int_z;
    float trigger_radius;
};

static SafehouseDoor g_Doors[] = {
    // 1. North Point Safehouse (landing on stairs & bedroom/living room)
    { "North Point Safehouse", 840.50f, -320.20f, 8.50f,   843.00f, -314.50f, 8.80f, 4.0f },
    // 2. Central Safehouse
    { "Central Safehouse",    -360.40f, -120.70f, 18.20f, -365.00f, -118.00f, 18.50f, 3.5f },
    // 3. Aberdeen Safehouse (Houseboat)
    { "Aberdeen Safehouse",   -650.10f, -780.40f, 4.80f,  -655.00f, -785.00f, 5.00f, 3.5f },
    // 4. Kennedy Town Safehouse (Luxury Penthouse)
    { "Kennedy Town Safehouse", -1150.30f, 120.50f, 25.10f, -1155.00f, 125.00f, 25.50f, 3.8f },
    // 5. Sifu Martial Arts Dojo (North Point)
    { "Sifu Dojo",            795.00f, -290.00f, 5.50f,   795.00f, -282.00f, 5.50f, 3.5f }
};

static const int NUM_DOORS = sizeof(g_Doors) / sizeof(g_Doors[0]);
static DWORD g_lastTeleportTime = 0;

//--------------------------------------------------------------------------
// Engine Hook: UpdateTransform
//--------------------------------------------------------------------------
typedef void(__fastcall* FnUpdateTransform)(void* pTransformNode);
static FnUpdateTransform g_oUpdateTransform = NULL;

static void TeleportPlayer(void* pSimObj, void* pTransformNode, float tx, float ty, float tz, const char* name, bool entering)
{
    // Update TransformNodeComponent matrices
    float* pWorld = (float*)((uintptr_t)pTransformNode + 0x80);
    float* pLocal = (float*)((uintptr_t)pTransformNode + 0xC0);
    pWorld[12] = tx; pWorld[13] = ty; pWorld[14] = tz;
    pLocal[12] = tx; pLocal[13] = ty; pLocal[14] = tz;
    *(int*)((uintptr_t)pTransformNode + 0x70) = 1; // mChanged = 1

    // Call native Teleport function
    typedef void(__fastcall* FnTeleport)(void* pSimObj, const float* pMatrix);
    static FnTeleport pTeleport = NULL;
    if (!pTeleport)
    {
        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        pTeleport = (FnTeleport)(base + 0x5BD2E0);
    }
    if (pTeleport)
    {
        pTeleport(pSimObj, pWorld);
    }

    // Audio cue
    MessageBeep(MB_OK);

    char logBuf[256];
    sprintf_s(logBuf, "[SafehouseHook] %s: %s (Target: X=%.2f, Y=%.2f, Z=%.2f)\n",
        entering ? "ENTRANDO A" : "SALIENDO DE", name, tx, ty, tz);
    OutputDebugStringA(logBuf);
}

static void OnPlayerTransform(void* pSimObj, void* pTransformNode)
{
    float* pWorld = (float*)((uintptr_t)pTransformNode + 0x80);
    float px = pWorld[12];
    float py = pWorld[13];
    float pz = pWorld[14];

    DWORD now = GetTickCount();
    if (now - g_lastTeleportTime < 1800)
    {
        return; // Cooldown to prevent instant bouncing
    }

    // Check interaction button
    bool isActionPressed = false;

    // Keyboard [E] or [Enter]
    if ((GetAsyncKeyState('E') & 0x8000) != 0 || (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
    {
        isActionPressed = true;
    }

    // Gamepad (DS4Windows / XInput): Triangle/Y or Cross/A
    if (!isActionPressed && g_pfnXInputGetState)
    {
        XINPUT_STATE xs = { 0 };
        if (g_pfnXInputGetState(0, &xs) == 0)
        {
            if (xs.Gamepad.wButtons & (XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X))
            {
                isActionPressed = true;
            }
        }
    }

    if (!isActionPressed)
    {
        return;
    }

    for (int i = 0; i < NUM_DOORS; ++i)
    {
        const SafehouseDoor& door = g_Doors[i];

        // Check distance to Exterior Door
        float dxExt = px - door.ext_x;
        float dyExt = py - door.ext_y;
        float dzExt = fabsf(pz - door.ext_z);
        float distExt = sqrtf(dxExt * dxExt + dyExt * dyExt);

        if (distExt <= door.trigger_radius && dzExt <= 6.0f)
        {
            g_lastTeleportTime = now;
            TeleportPlayer(pSimObj, pTransformNode, door.int_x, door.int_y, door.int_z, door.name, true);
            return;
        }

        // Check distance to Interior Door
        float dxInt = px - door.int_x;
        float dyInt = py - door.int_y;
        float dzInt = fabsf(pz - door.int_z);
        float distInt = sqrtf(dxInt * dxInt + dyInt * dyInt);

        if (distInt <= door.trigger_radius && dzInt <= 6.0f)
        {
            g_lastTeleportTime = now;
            TeleportPlayer(pSimObj, pTransformNode, door.ext_x, door.ext_y, door.ext_z, door.name, false);
            return;
        }
    }
}

static void __fastcall HookUpdateTransform(void* pTransformNode)
{
    if (pTransformNode)
    {
        // Offset 0x28 is m_pSimObject
        void* pSimObj = *(void**)((uintptr_t)pTransformNode + 0x28);
        if (pSimObj)
        {
            // Offset 0x50 is m_Name (qSymbol)
            uint32_t nameUID = *(uint32_t*)((uintptr_t)pSimObj + 0x50);
            if (nameUID == 0x90ECB5FF) // "PlayerOne_Havok" -> Local Player
            {
                OnPlayerTransform(pSimObj, pTransformNode);
            }
        }
    }
    g_oUpdateTransform(pTransformNode);
}

//--------------------------------------------------------------------------
// Initialization Thread
//--------------------------------------------------------------------------
static DWORD WINAPI InitThread(LPVOID lpParam)
{
    // Load XInput dynamically
    HMODULE hXInput = LoadLibraryA("Xinput1_4.dll");
    if (!hXInput) hXInput = LoadLibraryA("Xinput1_3.dll");
    if (!hXInput) hXInput = LoadLibraryA("Xinput9_1_0.dll");
    if (hXInput)
    {
        g_pfnXInputGetState = (PFN_XInputGetState)GetProcAddress(hXInput, "XInputGetState");
    }

    uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
    if (!base) return 0;

    MH_Initialize();

    // Hook UpdateTransform at RVA 0x1CA120
    void* targetFunc = (void*)(base + 0x1CA120);
    if (MH_CreateHook(targetFunc, (void*)&HookUpdateTransform, (void**)&g_oUpdateTransform) == MH_OK)
    {
        OutputDebugStringA("[SafehouseHook] Hooked UpdateTransform successfully at 0x1401CA120!\n");
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    return TRUE;
}
