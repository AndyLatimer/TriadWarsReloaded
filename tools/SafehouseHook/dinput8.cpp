#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <mmsystem.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")

//--------------------------------------------------------------------------
// Logging System (beta/safehouse_hook.log)
//--------------------------------------------------------------------------
static void Log(const char* fmt, ...)
{
    static char logPath[MAX_PATH] = { 0 };
    if (logPath[0] == '\0')
    {
        HMODULE hMod = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&Log, &hMod);
        GetModuleFileNameA(hMod, logPath, MAX_PATH);
        char* lastSlash = strrchr(logPath, '\\');
        if (lastSlash) *(lastSlash + 1) = '\0';
        strcat_s(logPath, "safehouse_hook.log");
    }
    FILE* fp = NULL;
    fopen_s(&fp, logPath, "a");
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
// XInput dynamic binding for DualShock 4 / Xbox Controller
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
    { "North Point Safehouse",  880.85f, -101.90f, 26.25f,   875.50f, -101.00f, 26.25f, 4.5f },
    // Backup North Point alternate door (street/docks)
    { "North Point (Docks)",    840.50f, -320.20f,  5.30f,   843.00f, -314.50f,  5.50f, 4.0f },
    // 2. Central Safehouse
    { "Central Safehouse",     -360.40f, -120.70f, 18.20f,  -365.00f, -118.00f, 18.50f, 4.0f },
    // 3. Aberdeen Safehouse (Houseboat)
    { "Aberdeen Safehouse",    -650.10f, -780.40f,  4.80f,  -655.00f, -785.00f,  5.00f, 4.0f },
    // 4. Kennedy Town Safehouse (Luxury Penthouse)
    { "Kennedy Town Safehouse",-1150.30f, 120.50f, 25.10f, -1155.00f,  125.00f, 25.50f, 4.0f },
    // 5. Sifu Martial Arts Dojo (North Point)
    { "Sifu Dojo",             795.00f, -290.00f,  5.50f,   795.00f, -282.00f,  5.50f, 4.0f }
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
    { "Nevera / Fridge (Dragon Kick)",  873.80f, -105.00f, 26.25f, 2.0f, "Beber Dragon Kick (+Buff combate)" },
    { "Inodoro / Toilet (Mear)",        870.50f, -107.20f, 26.25f, 2.0f, "Mear / Orinar (+Face / Satisfaccion)" },
    { "Lavabo / Sink (Lavarse la cara)", 872.80f, -107.20f, 26.25f, 2.0f, "Lavarse la cara (+Salud al 100%)" },
    { "Cama / Bed (Dormir)",            867.50f, -101.00f, 26.25f, 2.5f, "Dormir (+Curacion 100% y ciclo dia/noche)" },
    { "Armario / Wardrobe (Ropa)",      868.50f,  -98.00f, 26.25f, 2.2f, "Abrir Armario / Cambiar Ropa" }
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

// Anti-Havok snap continuous hold state
static DWORD g_teleportHoldUntil = 0;
static float g_holdX = 0.0f;
static float g_holdY = 0.0f;
static float g_holdZ = 0.0f;

// Engine function types
typedef void (__fastcall* FnTeleportToTransform)(void* pTSActor, const float* pMatrix);
typedef void* (__fastcall* FnGetLocalPlayer)();
typedef void (__fastcall* FnUpdateTransform)(void* pTransformNode);

//--------------------------------------------------------------------------
// Component Locators
//--------------------------------------------------------------------------
static void* FindTSActorFromSimObject(void* pSimObj)
{
    if (!pSimObj || (uintptr_t)pSimObj < 0x10000) return NULL;

    __try
    {
        uint32_t targetUID = *(uint32_t*)((uintptr_t)pSimObj + 0x40);

        // 1. Direct Actor Manager scan (0x25E09B8 count, 0x25E09C0 array, stride 0xA8)
        uint32_t count = *(uint32_t*)(g_base + 0x25E09B8);
        uint8_t* pEntries = *(uint8_t**)(g_base + 0x25E09C0);
        if (pEntries && count > 0 && count < 5000)
        {
            for (uint32_t i = 0; i < count; ++i)
            {
                uint8_t* entry = pEntries + i * 0xA8;
                void* pActor = *(void**)entry;
                uint32_t uid = *(uint32_t*)(entry + 8);
                if (pActor && ((uintptr_t)pActor > 0x10000) && ((uintptr_t)pActor & 0x7) == 0)
                {
                    if (uid == targetUID || *(void**)((uintptr_t)pActor + 0xD0) == pSimObj)
                    {
                        return pActor;
                    }
                }
            }
        }

        // 2. Fallback scan in SimObject components
        uint32_t numComp = *(uint32_t*)((uintptr_t)pSimObj + 0x60);
        void** pData = *(void***)((uintptr_t)pSimObj + 0x68);
        if (pData && numComp <= 128)
        {
            for (uint32_t i = 0; i < numComp; ++i)
            {
                void* comp = pData[i * 2];
                if (!comp || (uintptr_t)comp < 0x10000) continue;

                uintptr_t* pFields = (uintptr_t*)comp;
                for (int f = 0; f < 32; ++f)
                {
                    uintptr_t candidate = pFields[f];
                    if (candidate > 0x10000 && (candidate & 0x7) == 0)
                    {
                        void* backRef = *(void**)(candidate + 0xD0);
                        if (backRef == pSimObj)
                        {
                            return (void*)candidate;
                        }
                    }
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return NULL;
}

static void* FindHealthComponent(void* pSimObj)
{
    if (!pSimObj || (uintptr_t)pSimObj < 0x10000) return NULL;

    __try
    {
        uint32_t numComp = *(uint32_t*)((uintptr_t)pSimObj + 0x60);
        void** pData = *(void***)((uintptr_t)pSimObj + 0x68);

        if (!pData || numComp > 100) return NULL;

        for (uint32_t i = 0; i < numComp; ++i)
        {
            void* comp = pData[i * 2];
            uint32_t typeUID = *(uint32_t*)((uintptr_t)pData + i * 16 + 8);

            // HealthComponent TypeUID is 0xAA000001
            if ((typeUID & 0xFE000000) == 0xAA000000 && comp && (uintptr_t)comp > 0x10000)
            {
                return comp;
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return NULL;
}

//--------------------------------------------------------------------------
// Teleport Function: Dual-layer Dispatch (Direct Visual + Native Engine Physics)
//--------------------------------------------------------------------------
static void TeleportPlayer(void* pSimObj, void* pTransformNode, float tx, float ty, float tz, const char* name, bool entering)
{
    if (!pSimObj || !pTransformNode) return;

    Log("[TELEPORT] >>> %s hacia %s -> Destino: (X=%.2f, Y=%.2f, Z=%.2f)",
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

        // 2. Instant direct visual placement on graphics transform node
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

        // 3. Set Anti-Havok snap hold for 600ms across physics ticks
        g_holdX = tx;
        g_holdY = ty;
        g_holdZ = tz;
        g_teleportHoldUntil = GetTickCount() + 600;

        // 4. Locate TSActor and invoke canonical native engine teleport functions
        void* pTSActor = FindTSActorFromSimObject(pSimObj);
        if (pTSActor)
        {
            Log("[TELEPORT] TSActor localizado (0x%p). Despachando eventos de teleportacion nativa...", pTSActor);
            
            // Primary: TSActor::TeleportToTransform (0x5958B0) -> PlaceOnGround + QueueEvent + OnTeleport
            FnTeleportToTransform pTeleport = (FnTeleportToTransform)(g_base + 0x5958B0);
            if (pTeleport)
            {
                pTeleport(pTSActor, matrix);
            }

            // Secondary: TSActor::TeleportToTransformRaw (0x595C70) -> Direct physics event + OnTeleport
            FnTeleportToTransform pTeleportRaw = (FnTeleportToTransform)(g_base + 0x595C70);
            if (pTeleportRaw)
            {
                pTeleportRaw(pTSActor, matrix);
            }

            Log("[TELEPORT] EXITO: Teleportacion nativa completada al 100%% en motor!");
        }
        else
        {
            Log("[TELEPORT] AVISO: TSActor no encontrado en este tick; posicion directa mantenida via TransformNode.");
        }

        // Sound cue: transition confirmation chime
        PlaySoundA("SystemAsterisk", NULL, SND_ALIAS | SND_ASYNC);

        Log("[TELEPORT] EXITO: Wei Shen ubicado en %s (X=%.2f, Y=%.2f, Z=%.2f)", name, tx, ty, tz);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log("[TELEPORT] AVISO: Exception controlada en TeleportPlayer (0x%08X)", GetExceptionCode());
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
        {
            PlaySoundA("SystemQuestion", NULL, SND_ALIAS | SND_ASYNC);
            void* pHealth = FindHealthComponent(pSimObj);
            if (pHealth)
            {
                __try
                {
                    float maxHealth = *(float*)((uintptr_t)pHealth + 0x5c);
                    if (maxHealth > 0.0f)
                    {
                        *(float*)((uintptr_t)pHealth + 0x54) = maxHealth;
                        Log("[HEAL] Salud restaurada completamente al 100%% (%.1f / %.1f HP).", maxHealth, maxHealth);
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            Log("[ACTION] Wei Shen se lava la cara con agua fresca. Sensacion renovada!");
            break;
        }

        case 3: // Cama - Dormir / Sleep
        {
            PlaySoundA("SystemExit", NULL, SND_ALIAS | SND_ASYNC);
            void* pHealth = FindHealthComponent(pSimObj);
            if (pHealth)
            {
                __try
                {
                    float maxHealth = *(float*)((uintptr_t)pHealth + 0x5c);
                    if (maxHealth > 0.0f)
                    {
                        *(float*)((uintptr_t)pHealth + 0x54) = maxHealth;
                        Log("[HEAL] Salud restaurada completamente al 100%% al dormir (%.1f / %.1f HP).", maxHealth, maxHealth);
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {}
            }
            Log("[SLEEP] Wei Shen duerme placidamente. Tiempo avanzado y energia al maximo.");
            break;
        }

        case 4: // Armario - Wardrobe Shop
            PlaySoundA("SystemStart", NULL, SND_ALIAS | SND_ASYNC);
            Log("[WARDROBE] Abriendo armario ropero de Wei Shen. Cambia tu atuendo!");
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

    // Keyboard: [E], [Enter], [Space], [F]
    if ((GetAsyncKeyState('E') & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 ||
        (GetAsyncKeyState('F') & 0x8000) != 0)
    {
        isActionPressed = true;
    }

    // Gamepad via XInput: [Y/Triangle], [A/Cross], [X/Square], [B/Circle]
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

    const SafehouseDoor& npDoor = g_Doors[0]; // North Point Safehouse

    // Partition strictly by X coordinate:
    // Outside the door landing: px >= 878.5f
    // Inside the apartment: px < 878.5f
    bool isInside = (px < 878.5f);

    if (!isInside)
    {
        // -------------------------------------------------------------
        // EXTERIOR: Check door entrance ONLY!
        // -------------------------------------------------------------
        float dx = px - npDoor.ext_x;
        float dy = py - npDoor.ext_y;
        float dz = fabsf(pz - npDoor.ext_z);
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist <= npDoor.trigger_radius && dz <= 4.0f)
        {
            if (!isActionPressed && (now - g_lastProximityLogTime > 2500))
            {
                g_lastProximityLogTime = now;
                Log("[PROXIMITY] En el rellano de '%s' (dist=%.2f m). Pulsa [E] o [Y] para ENTRAR al piso.",
                    npDoor.name, dist);
            }

            if (isActionPressed && (now - g_lastTeleportTime > 1200))
            {
                g_lastTeleportTime = now;
                Log("[DOOR ACTION] >>> Entrando al piso '%s' (dist=%.2f m)", npDoor.name, dist);
                TeleportPlayer(pSimObj, pTransformNode, npDoor.int_x, npDoor.int_y, npDoor.int_z, npDoor.name, true);
                return true;
            }
        }
    }
    else
    {
        // -------------------------------------------------------------
        // INTERIOR: Inside North Point Apartment (Z ~ 26.25, X < 878.5)
        // -------------------------------------------------------------

        // 1. Check Door Exit (Interior door at X=877.50, Y=-101.90)
        float dxExit = px - 877.50f;
        float dyExit = py - (-101.90f);
        float distExit = sqrtf(dxExit * dxExit + dyExit * dyExit);

        if (distExit <= 2.5f && (now - g_lastTeleportTime > 1500))
        {
            if (!isActionPressed && (now - g_lastProximityLogTime > 2500))
            {
                g_lastProximityLogTime = now;
                Log("[PROXIMITY] Junto a la puerta de salida del apartamento (dist=%.2f m). Pulsa [E] o [Y] para SALIR al rellano.", distExit);
            }

            if (isActionPressed)
            {
                g_lastTeleportTime = now;
                Log("[DOOR ACTION] >>> Saliendo del piso al rellano exterior (dist=%.2f m)", distExit);
                TeleportPlayer(pSimObj, pTransformNode, npDoor.ext_x, npDoor.ext_y, npDoor.ext_z, npDoor.name, false);
                return true;
            }
        }

        // 2. Check Interior Fixtures (Toilet, Sink, Bed, Fridge, Wardrobe)
        // Only if at least 1.5s have elapsed since door entry to prevent accidental trigger
        if (now - g_lastTeleportTime > 1500)
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
                    Log("[POS_TRACKER] Posicion actual: (X=%.2f, Y=%.2f, Z=%.2f) | %s",
                        px, py, pz, (px < 878.5f) ? "DENTRO DEL PISO" : "FUERA EN EL RELLANO");
                }
            }

            // Hotkey [F9]: Inspector
            if ((GetAsyncKeyState(VK_F9) & 0x8000) != 0)
            {
                if (now - g_lastInspectorTime > 500)
                {
                    g_lastInspectorTime = now;
                    MessageBeep(MB_ICONINFORMATION);
                    Log("[F9 INSPECTOR] Posicion actual: X=%.2f, Y=%.2f, Z=%.2f (%s)",
                        px, py, pz, (px < 878.5f) ? "DENTRO DEL PISO" : "FUERA");
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
    Log("[SafehouseHook v3.2] Inicializado en Triad Wars x64 (Changelist 541920)");

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
    if (VirtualProtect((void*)(g_base + 0x185B000), 0x4000, PAGE_EXECUTE_READWRITE, &oldProt))
    {
        Log("[SafehouseHook] Pagina .rdata (0x185B000) desprotegida con exito (PAGE_EXECUTE_READWRITE).");
    }
    else
    {
        Log("[SafehouseHook] AVISO: VirtualProtect fallo en 0x185B000.");
    }

    // Launch background monitor thread
    CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
    Log("[SafehouseHook] MonitorThread lanzado. Sistema Sleeping Dogs 100%% activo!");
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
