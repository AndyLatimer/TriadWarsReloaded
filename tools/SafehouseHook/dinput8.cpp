#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

//--------------------------------------------------------------------------
// Logging System (real-time to beta/safehouse_hook.log)
//--------------------------------------------------------------------------
static void Log(const char* fmt, ...)
{
    FILE* fp = NULL;
    fopen_s(&fp, "safehouse_hook.log", "a");
    if (!fp) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(fp, "[%02d:%02d:%02d.%03d] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fprintf(fp, "\n");
    fclose(fp);
}

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
            Log("[dinput8] Proxy encadenado a dinput8.dll original del sistema (%s)", sysPath);
        }
        else
        {
            Log("[dinput8] ERROR: No se pudo cargar dinput8.dll del sistema!");
        }
    }
    if (g_oDirectInput8Create)
    {
        return g_oDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
    return E_FAIL;
}

//--------------------------------------------------------------------------
// Safehouse Definitions (Exact coordinates from missions_data.json)
//--------------------------------------------------------------------------
struct SafehouseDoor {
    const char* name;
    float ext_x, ext_y, ext_z;
    float int_x, int_y, int_z;
    float trigger_radius;
};

static SafehouseDoor g_Doors[] = {
    // 1. North Point Safehouse (exterior landing on stairs vs apartment interior)
    { "North Point Safehouse",  840.50f, -320.20f, 5.30f,   843.00f, -314.50f, 5.50f, 6.0f },
    // 2. Central Safehouse
    { "Central Safehouse",     -360.40f, -120.70f, 18.20f, -365.00f, -118.00f, 18.50f, 5.0f },
    // 3. Aberdeen Safehouse (Houseboat)
    { "Aberdeen Safehouse",    -650.10f, -780.40f, 4.80f,  -655.00f, -785.00f, 5.00f, 5.0f },
    // 4. Kennedy Town Safehouse (Luxury Penthouse)
    { "Kennedy Town Safehouse",-1150.30f, 120.50f, 25.10f, -1155.00f, 125.00f, 25.50f, 5.0f },
    // 5. Sifu Martial Arts Dojo (North Point)
    { "Sifu Dojo",             795.00f, -290.00f, 5.50f,   795.00f, -282.00f, 5.50f, 5.0f }
};

static const int NUM_DOORS = sizeof(g_Doors) / sizeof(g_Doors[0]);
static uintptr_t g_base = 0;
static DWORD g_lastTeleportTime = 0;
static DWORD g_lastProximityLogTime = 0;
static DWORD g_lastInspectorTime = 0;
static DWORD g_lastPeriodicLogTime = 0;
static float g_lastPeriodicX = 0.0f;
static float g_lastPeriodicY = 0.0f;
static float g_lastPeriodicZ = 0.0f;
static volatile bool g_bRunning = true;

// Engine function types
typedef void* (__fastcall* FnMalloc)(size_t size, const char* name, uint32_t flags);
typedef void* (__fastcall* FnTeleportEventCtor)(void* pMem, const float* pMatrix, uint32_t targetUID, uint32_t r9, uint64_t channel, uint8_t unk);
typedef void (__fastcall* FnQueueEvent)(void* pDispatcher, void* pEvent);
typedef void* (__fastcall* FnGetLocalPlayer)();
typedef void (__fastcall* FnUpdateTransform)(void* pTransformNode);

static void TeleportPlayer(void* pSimObj, void* pTransformNode, float tx, float ty, float tz, const char* name, bool entering)
{
    if (!pSimObj || !pTransformNode) return;

    Log("[TELEPORT] Iniciando teleport hacia %s (%s) -> Destino: (X=%.2f, Y=%.2f, Z=%.2f)",
        name, entering ? "INTERIOR" : "EXTERIOR", tx, ty, tz);

    __try
    {
        // 1. Build destination 4x4 matrix
        float matrix[16];
        memcpy(matrix, (void*)((uintptr_t)pTransformNode + 0x80), sizeof(matrix));
        if (matrix[0] == 0.0f && matrix[5] == 0.0f && matrix[10] == 0.0f)
        {
            memset(matrix, 0, sizeof(matrix));
            matrix[0] = 1.0f;
            matrix[5] = 1.0f;
            matrix[10] = 1.0f;
        }
        matrix[12] = tx;
        matrix[13] = ty;
        matrix[14] = tz;
        matrix[15] = 1.0f;

        // 2. Direct coordinate update on TransformNodeComponent
        float* pWorld = (float*)((uintptr_t)pTransformNode + 0x80);
        float* pLocal = (float*)((uintptr_t)pTransformNode + 0xC0);
        pWorld[12] = tx; pWorld[13] = ty; pWorld[14] = tz;
        pLocal[12] = tx; pLocal[13] = ty; pLocal[14] = tz;
        *(int*)((uintptr_t)pTransformNode + 0x70) = 1; // mChanged = 1

        FnUpdateTransform pUpdate = (FnUpdateTransform)(g_base + 0x1CA120);
        if (pUpdate)
        {
            pUpdate(pTransformNode);
            Log("[TELEPORT] Step 1: TransformNodeComponent actualizado.");
        }

        // 3. TheoryEngine canonical TeleportPosition event dispatch
        FnMalloc pMalloc = (FnMalloc)(g_base + 0x1BF580);
        FnTeleportEventCtor pTeleportEventCtor = (FnTeleportEventCtor)(g_base + 0x445290);
        FnQueueEvent pQueueEvent = (FnQueueEvent)(g_base + 0x1F0260);
        void* pDispatcher = (void*)(g_base + 0x23F5870);
        uint64_t* pChannelPtr = (uint64_t*)(g_base + 0x229F128);

        if (pMalloc && pTeleportEventCtor && pQueueEvent && pDispatcher && pChannelPtr)
        {
            void* pEventMem = pMalloc(0x90, "TeleportPosition", 0);
            if (pEventMem)
            {
                uint32_t targetUID = *(uint32_t*)((uintptr_t)pSimObj + 0x40);
                uint64_t channel = *pChannelPtr;

                void* pEvent = pTeleportEventCtor(pEventMem, matrix, targetUID, 0, channel, 0);
                if (pEvent)
                {
                    pQueueEvent(pDispatcher, pEvent);
                    Log("[TELEPORT] Step 2: Evento TeleportPosition despachado y encolado (UID=0x%08X, Channel=0x%llX).",
                        targetUID, channel);
                }
            }
            else
            {
                Log("[TELEPORT] Step 2 AVISO: qMalloc devolvio NULL, usando transform directo.");
            }
        }

        // Audio cue for successful teleport
        MessageBeep(MB_OK);

        Log("[TELEPORT] EXITO COMPLETO: Jugador teleportado a %s (X=%.2f, Y=%.2f, Z=%.2f)",
            name, tx, ty, tz);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("[TELEPORT] Excepcion capturada durante TeleportPlayer (Code=0x%08X)", GetExceptionCode());
    }
}

static bool CheckSafehouseProximity(void* pSimObj, void* pTransformNode, float px, float py, float pz, bool requireActionKey)
{
    if (!pSimObj || !pTransformNode) return false;

    DWORD now = GetTickCount();

    // Check interaction button
    bool isActionPressed = false;

    if (requireActionKey)
    {
        // Keyboard: [E] or [Enter]
        if ((GetAsyncKeyState('E') & 0x8000) != 0 || (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
        {
            isActionPressed = true;
        }

        // Gamepad via XInput: [Y/Triangle], [A/Cross], or [X/Square]
        if (!isActionPressed && g_pfnXInputGetState)
        {
            XINPUT_STATE xs = { 0 };
            for (DWORD padIdx = 0; padIdx < 4; ++padIdx)
            {
                if (g_pfnXInputGetState(padIdx, &xs) == 0)
                {
                    if (xs.Gamepad.wButtons & (XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X))
                    {
                        isActionPressed = true;
                        break;
                    }
                }
            }
        }

        if (!isActionPressed)
        {
            // If not pressed, log proximity when near door (every 2.5s)
            if (now - g_lastProximityLogTime > 2500)
            {
                for (int i = 0; i < NUM_DOORS; ++i)
                {
                    const SafehouseDoor& door = g_Doors[i];
                    float dExt = sqrtf((px - door.ext_x)*(px - door.ext_x) + (py - door.ext_y)*(py - door.ext_y));
                    float dInt = sqrtf((px - door.int_x)*(px - door.int_x) + (py - door.int_y)*(py - door.int_y));
                    if (dExt <= door.trigger_radius + 3.0f || dInt <= door.trigger_radius + 3.0f)
                    {
                        g_lastProximityLogTime = now;
                        Log("[PROXIMITY] Cerca de '%s' | Pos: (%.2f, %.2f, %.2f) | Dist Ext: %.2f m, Dist Int: %.2f m. Pulsa [E] o [Y].",
                            door.name, px, py, pz, dExt, dInt);
                        break;
                    }
                }
            }
            return false;
        }
    }

    if (now - g_lastTeleportTime < 2000)
    {
        return false; // Cooldown prevent bounce loop
    }

    for (int i = 0; i < NUM_DOORS; ++i)
    {
        const SafehouseDoor& door = g_Doors[i];

        // Exterior door check
        float dxExt = px - door.ext_x;
        float dyExt = py - door.ext_y;
        float dzExt = fabsf(pz - door.ext_z);
        float distExt = sqrtf(dxExt * dxExt + dyExt * dyExt);

        if (distExt <= door.trigger_radius && dzExt <= 6.0f)
        {
            g_lastTeleportTime = now;
            Log("[PROXIMITY TRIGGER] Accion activada en puerta exterior de '%s' (dist=%.2f m, dZ=%.2f)",
                door.name, distExt, dzExt);
            TeleportPlayer(pSimObj, pTransformNode, door.int_x, door.int_y, door.int_z, door.name, true);
            return true;
        }

        // Interior door check
        float dxInt = px - door.int_x;
        float dyInt = py - door.int_y;
        float dzInt = fabsf(pz - door.int_z);
        float distInt = sqrtf(dxInt * dxInt + dyInt * dyInt);

        if (distInt <= door.trigger_radius && dzInt <= 6.0f)
        {
            g_lastTeleportTime = now;
            Log("[PROXIMITY TRIGGER] Accion activada en puerta interior de '%s' (dist=%.2f m, dZ=%.2f)",
                door.name, distInt, dzInt);
            TeleportPlayer(pSimObj, pTransformNode, door.ext_x, door.ext_y, door.ext_z, door.name, false);
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------
// Background Monitor Thread: Active polling via native GetLocalPlayer() (RVA 0x3978A0)
//--------------------------------------------------------------------------
static DWORD WINAPI MonitorThread(LPVOID lpParam)
{
    Log("[MonitorThread] Hilo de monitorizacion nativa iniciado.");

    FnGetLocalPlayer pGetLocalPlayer = (FnGetLocalPlayer)(g_base + 0x3978A0);
    FnUpdateTransform pUpdateTransform = (FnUpdateTransform)(g_base + 0x1CA120);
    Log("[MonitorThread] GetLocalPlayer resuelto en 0x%p, UpdateTransform resuelto en 0x%p",
        (void*)pGetLocalPlayer, (void*)pUpdateTransform);

    while (g_bRunning)
    {
        Sleep(30);

        DWORD now = GetTickCount();

        // 1. Resolve local player directly from TheoryEngine
        void* pSimObj = NULL;
        __try
        {
            if (pGetLocalPlayer)
            {
                pSimObj = pGetLocalPlayer();
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            pSimObj = NULL;
        }

        if (!pSimObj || (uintptr_t)pSimObj < 0x10000)
        {
            continue;
        }

        // 2. Resolve TransformNodeComponent from SimObject (+0x58)
        void* pTrans = NULL;
        __try
        {
            pTrans = *(void**)((uintptr_t)pSimObj + 0x58);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            pTrans = NULL;
        }

        if (!pTrans || (uintptr_t)pTrans < 0x10000)
        {
            continue;
        }

        __try
        {
            // CRUCIAL: Call UpdateWorldTransform to evaluate lazy transform
            if (pUpdateTransform)
            {
                pUpdateTransform(pTrans);
            }

            float* pWorld = (float*)((uintptr_t)pTrans + 0x80);
            float px = pWorld[12];
            float py = pWorld[13];
            float pz = pWorld[14];

            // Periodic position tracker (logs position every 4 seconds when player moves)
            if (now - g_lastPeriodicLogTime > 4000)
            {
                float dMoved = sqrtf((px - g_lastPeriodicX)*(px - g_lastPeriodicX) + (py - g_lastPeriodicY)*(py - g_lastPeriodicY));
                if (dMoved > 1.5f || (g_lastPeriodicX == 0.0f && (px != 0.0f || py != 0.0f)))
                {
                    g_lastPeriodicLogTime = now;
                    g_lastPeriodicX = px;
                    g_lastPeriodicY = py;
                    g_lastPeriodicZ = pz;
                    Log("[POS_TRACKER] Posicion actual: (X=%.2f, Y=%.2f, Z=%.2f)", px, py, pz);
                }
            }

            // Hotkey [F9]: Coordinates Inspector & Proximity Status
            if ((GetAsyncKeyState(VK_F9) & 0x8000) != 0)
            {
                if (now - g_lastInspectorTime > 500)
                {
                    g_lastInspectorTime = now;
                    MessageBeep(MB_ICONINFORMATION);
                    Log("[F9 INSPECTOR] Posicion actual del jugador: X=%.2f, Y=%.2f, Z=%.2f", px, py, pz);
                    for (int i = 0; i < NUM_DOORS; ++i)
                    {
                        const SafehouseDoor& d = g_Doors[i];
                        float distExt = sqrtf((px - d.ext_x)*(px - d.ext_x) + (py - d.ext_y)*(py - d.ext_y));
                        float distInt = sqrtf((px - d.int_x)*(px - d.int_x) + (py - d.int_y)*(py - d.int_y));
                        Log("   -> %s: Dist Ext=%.2f m (dZ=%.2f), Dist Int=%.2f m (dZ=%.2f)",
                            d.name, distExt, fabsf(pz - d.ext_z), distInt, fabsf(pz - d.int_z));
                    }
                }
            }

            // Hotkey [F7]: Forzar Teleport Directo al interior del Safehouse de North Point
            if ((GetAsyncKeyState(VK_F7) & 0x8000) != 0)
            {
                if (now - g_lastTeleportTime > 1200)
                {
                    g_lastTeleportTime = now;
                    Log("[F7 HOTKEY] Forzando entrada al piso franco de North Point...");
                    TeleportPlayer(pSimObj, pTrans, g_Doors[0].int_x, g_Doors[0].int_y, g_Doors[0].int_z, g_Doors[0].name, true);
                }
            }

            // Hotkey [F8]: Forzar Teleport Directo al exterior del Safehouse de North Point
            if ((GetAsyncKeyState(VK_F8) & 0x8000) != 0)
            {
                if (now - g_lastTeleportTime > 1200)
                {
                    g_lastTeleportTime = now;
                    Log("[F8 HOTKEY] Forzando salida al rellano de North Point...");
                    TeleportPlayer(pSimObj, pTrans, g_Doors[0].ext_x, g_Doors[0].ext_y, g_Doors[0].ext_z, g_Doors[0].name, false);
                }
            }

            // Check proximity and standard interaction keys [E] / [Y]
            CheckSafehouseProximity(pSimObj, pTrans, px, py, pz, true);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    return 0;
}

//--------------------------------------------------------------------------
// Initialization Thread
//--------------------------------------------------------------------------
static DWORD WINAPI InitThread(LPVOID lpParam)
{
    Log("=================================================");
    Log("[SafehouseHook] Inyectado en Triad Wars x64 (Changelist 541920)");

    // Dynamically load XInput
    HMODULE hXInput = LoadLibraryA("Xinput1_4.dll");
    if (!hXInput) hXInput = LoadLibraryA("Xinput1_3.dll");
    if (!hXInput) hXInput = LoadLibraryA("Xinput9_1_0.dll");
    if (hXInput)
    {
        g_pfnXInputGetState = (PFN_XInputGetState)GetProcAddress(hXInput, "XInputGetState");
        Log("[SafehouseHook] XInput cargado con exito para compatibilidad mando DS4/Xbox.");
    }

    g_base = (uintptr_t)GetModuleHandleA(NULL);
    if (!g_base)
    {
        Log("[SafehouseHook] ERROR: No se pudo obtener base address de TriadWars.exe!");
        return 0;
    }
    Log("[SafehouseHook] TriadWars.exe base address = 0x%p", (void*)g_base);

    // Launch background monitor thread
    CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
    Log("[SafehouseHook] MonitorThread lanzado. Listo!");
    Log("=================================================");

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        g_bRunning = false;
    }
    return TRUE;
}
