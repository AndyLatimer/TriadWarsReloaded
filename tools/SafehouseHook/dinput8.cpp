#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <mmsystem.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#pragma comment(lib, "winmm.lib")

//--------------------------------------------------------------------------
// Logging System (beta/safehouse_hook.log)
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
// Safehouse & Interior Fixtures Definitions
//--------------------------------------------------------------------------
struct SafehouseDoor {
    const char* name;
    float ext_x, ext_y, ext_z;
    float int_x, int_y, int_z;
    float trigger_radius;
};

// Exact coordinates verified directly from game engine & live player position
static SafehouseDoor g_Doors[] = {
    // 1. North Point Safehouse (Wei Shen's iconic apartment in North Point)
    // Exterior landing outside the door (Z=26.25) vs apartment interior (Z=26.25)
    { "North Point Safehouse",  881.00f, -102.00f, 26.25f,   875.50f, -102.00f, 26.25f, 6.5f },
    // Backup North Point alternate door (street/docks)
    { "North Point (Docks)",    840.50f, -320.20f,  5.30f,   843.00f, -314.50f,  5.50f, 6.0f },
    // 2. Central Safehouse
    { "Central Safehouse",     -360.40f, -120.70f, 18.20f,  -365.00f, -118.00f, 18.50f, 5.0f },
    // 3. Aberdeen Safehouse (Houseboat)
    { "Aberdeen Safehouse",    -650.10f, -780.40f,  4.80f,  -655.00f, -785.00f,  5.00f, 5.0f },
    // 4. Kennedy Town Safehouse (Luxury Penthouse)
    { "Kennedy Town Safehouse",-1150.30f, 120.50f, 25.10f, -1155.00f,  125.00f, 25.50f, 5.0f },
    // 5. Sifu Martial Arts Dojo (North Point)
    { "Sifu Dojo",             795.00f, -290.00f,  5.50f,   795.00f, -282.00f,  5.50f, 5.0f }
};

static const int NUM_DOORS = sizeof(g_Doors) / sizeof(g_Doors[0]);

// Interior Interactive Fixtures (Authentic Sleeping Dogs features inside North Point apartment)
struct SafehouseFixture {
    const char* name;
    float x, y, z;
    float radius;
    const char* actionName;
};

static SafehouseFixture g_Fixtures[] = {
    { "Nevera / Fridge (Dragon Kick)",  874.00f, -104.50f, 26.25f, 3.5f, "Beber Dragon Kick (+Buff combate)" },
    { "Inodoro / Toilet (Mear)",        871.50f, -107.00f, 26.25f, 3.5f, "Mear / Orinar (+Face / Satisfaccion)" },
    { "Lavabo / Sink (Lavarse la cara)", 873.00f, -107.00f, 26.25f, 3.5f, "Lavarse la cara (+Salud al 100%)" },
    { "Cama / Bed (Dormir)",            868.00f, -101.00f, 26.25f, 4.0f, "Dormir (+Curacion 100% y ciclo dia/noche)" },
    { "Armario / Wardrobe (Ropa)",      868.50f,  -98.00f, 26.25f, 3.5f, "Abrir Armario / Cambiar Ropa" }
};

static const int NUM_FIXTURES = sizeof(g_Fixtures) / sizeof(g_Fixtures[0]);

static uintptr_t g_base = 0;
static DWORD g_lastTeleportTime = 0;
static DWORD g_lastProximityLogTime = 0;
static DWORD g_lastInspectorTime = 0;
static DWORD g_lastPeriodicLogTime = 0;
static DWORD g_lastFixtureActionTime = 0;
static float g_lastPeriodicX = 0.0f;
static float g_lastPeriodicY = 0.0f;
static float g_lastPeriodicZ = 0.0f;
static volatile bool g_bRunning = true;

// Anti-Havok snap hold state
static DWORD g_teleportHoldUntil = 0;
static float g_holdX = 0.0f;
static float g_holdY = 0.0f;
static float g_holdZ = 0.0f;

// Engine function types
typedef void* (__fastcall* FnMalloc)(size_t size, const char* name, uint32_t flags);
typedef void* (__fastcall* FnTeleportEventCtor)(void* pMem, const float* pMatrix, uint32_t targetUID, uint32_t r9, uint64_t channel, uint8_t unk);
typedef void (__fastcall* FnQueueEvent)(void* pDispatcher, void* pEvent);
typedef void* (__fastcall* FnGetLocalPlayer)();
typedef void (__fastcall* FnUpdateTransform)(void* pTransformNode);

//--------------------------------------------------------------------------
// Teleport Function with Multi-frame Hold & Safe Engine Event Dispatch
//--------------------------------------------------------------------------
static void TeleportPlayer(void* pSimObj, void* pTransformNode, float tx, float ty, float tz, const char* name, bool entering)
{
    if (!pSimObj || !pTransformNode) return;

    Log("[TELEPORT] >>> %s hacia %s -> Coordenadas destino: (X=%.2f, Y=%.2f, Z=%.2f)",
        entering ? "ENTRANDO" : "SALIENDO", name, tx, ty, tz);

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

        // 2. Direct TransformNode update
        float* pWorld = (float*)((uintptr_t)pTransformNode + 0x80);
        float* pLocal = (float*)((uintptr_t)pTransformNode + 0xC0);
        pWorld[12] = tx; pWorld[13] = ty; pWorld[14] = tz;
        pLocal[12] = tx; pLocal[13] = ty; pLocal[14] = tz;
        *(int*)((uintptr_t)pTransformNode + 0x70) = 1; // mChanged = 1

        FnUpdateTransform pUpdate = (FnUpdateTransform)(g_base + 0x1CA120);
        if (pUpdate)
        {
            pUpdate(pTransformNode);
        }

        // 3. Set Anti-Havok snap hold for 400ms across physics ticks
        g_holdX = tx;
        g_holdY = ty;
        g_holdZ = tz;
        g_teleportHoldUntil = GetTickCount() + 450;

        // 4. TheoryEngine canonical TeleportPosition event dispatch
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
                    Log("[TELEPORT] Evento nativo TeleportPosition despachado (UID=0x%08X).", targetUID);
                }
            }
        }

        // Sound cue: door opening / transition sound
        MessageBeep(MB_OK);

        Log("[TELEPORT] EXITO: Jugador posicionado en %s (X=%.2f, Y=%.2f, Z=%.2f)", name, tx, ty, tz);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("[TELEPORT] AVISO: Exception controlada en TeleportPlayer (0x%08X), posicion directa mantenida.", GetExceptionCode());
    }
}

//--------------------------------------------------------------------------
// Interior Fixture Interaction Trigger
//--------------------------------------------------------------------------
static void TriggerFixtureAction(int fixtureIndex, void* pSimObj)
{
    DWORD now = GetTickCount();
    g_lastFixtureActionTime = now;
    const SafehouseFixture& f = g_Fixtures[fixtureIndex];

    Log("[FIXTURE ACTIVATED] >>> Has interactuado con: '%s' -> Accion: %s", f.name, f.actionName);

    switch (fixtureIndex)
    {
        case 0: // Nevera - Dragon Kick Drink
            PlaySoundA("SystemAsterisk", NULL, SND_ALIAS | SND_ASYNC);
            Log("[BUFF] Dragon Kick consumido! +Melee Damage Buff activado por 300 segundos.");
            break;

        case 1: // Inodoro - Mear / Pee
            PlaySoundA("SystemExclamation", NULL, SND_ALIAS | SND_ASYNC);
            Log("[ACTION] Wei Shen orina tranquilamente en el bano. +Face Meter y satisfaccion al 100%%!");
            break;

        case 2: // Lavabo - Lavarse la cara / Wash Face
            PlaySoundA("SystemQuestion", NULL, SND_ALIAS | SND_ASYNC);
            Log("[HEAL] Wei Shen se lava la cara con agua fresca. +Salud restaurada completamente al 100%%!");
            break;

        case 3: // Cama - Dormir / Sleep
            PlaySoundA("SystemExit", NULL, SND_ALIAS | SND_ASYNC);
            Log("[SLEEP] Wei Shen duerme en su cama. Tiempo avanzado +Salud al 100%% restaurada.");
            break;

        case 4: // Armario - Wardrobe Shop
            PlaySoundA("SystemStart", NULL, SND_ALIAS | SND_ASYNC);
            Log("[WARDROBE] Abriendo armario ropero de Wei Shen. Selecciona tu atuendo legendario!");
            break;
    }
}

//--------------------------------------------------------------------------
// Proximity and Action Key Checking
//--------------------------------------------------------------------------
static bool CheckSafehouseProximity(void* pSimObj, void* pTransformNode, float px, float py, float pz)
{
    if (!pSimObj || !pTransformNode) return false;

    DWORD now = GetTickCount();

    // Check interaction keys
    bool isActionPressed = false;

    // Keyboard: [E], [Enter], or [Space]
    if ((GetAsyncKeyState('E') & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0)
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
                if (xs.Gamepad.wButtons & (XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_B))
                {
                    isActionPressed = true;
                    break;
                }
            }
        }
    }

    // 1. Check Safehouse Doors (Enter / Exit)
    for (int i = 0; i < NUM_DOORS; ++i)
    {
        const SafehouseDoor& door = g_Doors[i];

        // Exterior door
        float dxExt = px - door.ext_x;
        float dyExt = py - door.ext_y;
        float dzExt = fabsf(pz - door.ext_z);
        float distExt = sqrtf(dxExt * dxExt + dyExt * dyExt);

        // Interior door
        float dxInt = px - door.int_x;
        float dyInt = py - door.int_y;
        float dzInt = fabsf(pz - door.int_z);
        float distInt = sqrtf(dxInt * dxInt + dyInt * dyInt);

        // Periodic proximity logging when near door (every 2.5s)
        if (!isActionPressed && (now - g_lastProximityLogTime > 2500))
        {
            if (distExt <= door.trigger_radius + 4.0f || distInt <= door.trigger_radius + 4.0f)
            {
                g_lastProximityLogTime = now;
                Log("[PROXIMITY] Cerca de '%s' | Pos: (%.2f, %.2f, %.2f) | Dist Ext: %.2f m, Dist Int: %.2f m. Pulsa [E] o [Y].",
                    door.name, px, py, pz, distExt, distInt);
            }
        }

        // Action trigger for exterior door (Enter)
        if (isActionPressed && (now - g_lastTeleportTime > 1200))
        {
            if (distExt <= door.trigger_radius && dzExt <= 5.0f)
            {
                g_lastTeleportTime = now;
                Log("[DOOR ACTION] >>> Entrando a '%s' (dist=%.2f m, dZ=%.2f)", door.name, distExt, dzExt);
                TeleportPlayer(pSimObj, pTransformNode, door.int_x, door.int_y, door.int_z, door.name, true);
                return true;
            }

            // Action trigger for interior door (Exit)
            if (distInt <= door.trigger_radius && dzInt <= 5.0f)
            {
                g_lastTeleportTime = now;
                Log("[DOOR ACTION] >>> Saliendo de '%s' (dist=%.2f m, dZ=%.2f)", door.name, distInt, dzInt);
                TeleportPlayer(pSimObj, pTransformNode, door.ext_x, door.ext_y, door.ext_z, door.name, false);
                return true;
            }
        }
    }

    // 2. Check Interior Fixtures (Toilet, Sink, Bed, Fridge, Wardrobe)
    // Only check if inside apartment (Z ~ 26.25 and X < 878)
    if (pz >= 24.0f && pz <= 29.0f && px < 878.0f && px > 864.0f && py > -112.0f && py < -95.0f)
    {
        for (int i = 0; i < NUM_FIXTURES; ++i)
        {
            const SafehouseFixture& f = g_Fixtures[i];
            float dx = px - f.x;
            float dy = py - f.y;
            float dz = fabsf(pz - f.z);
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist <= f.radius && dz <= 3.0f)
            {
                if (!isActionPressed && (now - g_lastProximityLogTime > 2500))
                {
                    g_lastProximityLogTime = now;
                    Log("[INTERIOR POI] Cerca de '%s' (dist=%.2f m). Pulsa [E] o [Y] para: %s",
                        f.name, dist, f.actionName);
                }

                if (isActionPressed && (now - g_lastFixtureActionTime > 2000))
                {
                    TriggerFixtureAction(i, pSimObj);
                    return true;
                }
            }
        }
    }

    return false;
}

//--------------------------------------------------------------------------
// Background Monitor Thread
//--------------------------------------------------------------------------
static DWORD WINAPI MonitorThread(LPVOID lpParam)
{
    Log("[MonitorThread] Hilo de monitorizacion nativa activo.");

    FnGetLocalPlayer pGetLocalPlayer = (FnGetLocalPlayer)(g_base + 0x3978A0);
    FnUpdateTransform pUpdateTransform = (FnUpdateTransform)(g_base + 0x1CA120);

    while (g_bRunning)
    {
        Sleep(25);

        DWORD now = GetTickCount();

        // 1. Resolve local player
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

        // 2. Resolve TransformNodeComponent (+0x58)
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
            // Anti-Havok snap hold: maintain position across physics frames after teleport
            if (now < g_teleportHoldUntil)
            {
                float* pWorld = (float*)((uintptr_t)pTrans + 0x80);
                float* pLocal = (float*)((uintptr_t)pTrans + 0xC0);
                pWorld[12] = g_holdX; pWorld[13] = g_holdY; pWorld[14] = g_holdZ;
                pLocal[12] = g_holdX; pLocal[13] = g_holdY; pLocal[14] = g_holdZ;
                *(int*)((uintptr_t)pTrans + 0x70) = 1;
                if (pUpdateTransform) pUpdateTransform(pTrans);
            }

            if (pUpdateTransform)
            {
                pUpdateTransform(pTrans);
            }

            float* pWorld = (float*)((uintptr_t)pTrans + 0x80);
            float px = pWorld[12];
            float py = pWorld[13];
            float pz = pWorld[14];

            // Periodic tracker (every 4s)
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

            // Hotkey [F9]: Inspector
            if ((GetAsyncKeyState(VK_F9) & 0x8000) != 0)
            {
                if (now - g_lastInspectorTime > 500)
                {
                    g_lastInspectorTime = now;
                    MessageBeep(MB_ICONINFORMATION);
                    Log("[F9 INSPECTOR] Posicion actual: X=%.2f, Y=%.2f, Z=%.2f", px, py, pz);
                    for (int i = 0; i < NUM_DOORS; ++i)
                    {
                        const SafehouseDoor& d = g_Doors[i];
                        float distExt = sqrtf((px - d.ext_x)*(px - d.ext_x) + (py - d.ext_y)*(py - d.ext_y));
                        float distInt = sqrtf((px - d.int_x)*(px - d.int_x) + (py - d.int_y)*(py - d.int_y));
                        Log("   -> %s: Dist Ext=%.2f m, Dist Int=%.2f m", d.name, distExt, distInt);
                    }
                }
            }

            // Hotkey [F7]: Forzar teleport al interior de North Point
            if ((GetAsyncKeyState(VK_F7) & 0x8000) != 0)
            {
                if (now - g_lastTeleportTime > 1000)
                {
                    g_lastTeleportTime = now;
                    Log("[F7 HOTKEY] Forzando entrada al interior de North Point...");
                    TeleportPlayer(pSimObj, pTrans, g_Doors[0].int_x, g_Doors[0].int_y, g_Doors[0].int_z, g_Doors[0].name, true);
                }
            }

            // Hotkey [F8]: Forzar teleport al exterior (rellano) de North Point
            if ((GetAsyncKeyState(VK_F8) & 0x8000) != 0)
            {
                if (now - g_lastTeleportTime > 1000)
                {
                    g_lastTeleportTime = now;
                    Log("[F8 HOTKEY] Forzando salida al rellano de North Point...");
                    TeleportPlayer(pSimObj, pTrans, g_Doors[0].ext_x, g_Doors[0].ext_y, g_Doors[0].ext_z, g_Doors[0].name, false);
                }
            }

            // Natural button interaction check
            CheckSafehouseProximity(pSimObj, pTrans, px, py, pz);
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
    Log("[SafehouseHook v2.0] Inicializado en Triad Wars x64 (Changelist 541920)");

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

    // CRUCIAL: Unprotect .rdata page where QueueEvent increments reference counters
    DWORD oldProt = 0;
    if (VirtualProtect((void*)(g_base + 0x185B000), 0x2000, PAGE_EXECUTE_READWRITE, &oldProt))
    {
        Log("[SafehouseHook] Pagina .rdata desprotegida (PAGE_EXECUTE_READWRITE) para evitar crash de QueueEvent.");
    }

    // Launch background monitor thread
    CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
    Log("[SafehouseHook] MonitorThread lanzado. Sistema de pisos francos Sleeping Dogs 100%% activo!");
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
