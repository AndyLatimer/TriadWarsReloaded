#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

// XInput definitions
#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30

#define XINPUT_DEVTYPE_GAMEPAD          0x01
#define XINPUT_DEVSUBTYPE_GAMEPAD       0x01

typedef struct _XINPUT_GAMEPAD {
    WORD  wButtons;
    BYTE  bLeftTrigger;
    BYTE  bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE {
    DWORD          dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION {
    WORD wLeftMotorSpeed;
    WORD wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;

typedef struct _XINPUT_CAPABILITIES {
    BYTE             Type;
    BYTE             SubType;
    WORD             Flags;
    XINPUT_GAMEPAD   Gamepad;
    XINPUT_VIBRATION Vibration;
} XINPUT_CAPABILITIES, *PXINPUT_CAPABILITIES;

typedef DWORD (WINAPI *PFN_XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD (WINAPI *PFN_XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
typedef DWORD (WINAPI *PFN_XInputGetCapabilities)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
typedef DWORD (WINAPI *PFN_XInputGetDSoundAudioDeviceGuids)(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid);

static HMODULE g_hRealXInput = NULL;
static PFN_XInputGetState g_pfnGetState = NULL;
static PFN_XInputSetState g_pfnSetState = NULL;
static PFN_XInputGetCapabilities g_pfnGetCaps = NULL;
static PFN_XInputGetDSoundAudioDeviceGuids g_pfnGetDSound = NULL;

static DWORD g_dwPacketNumber = 0;

static void InitRealXInput()
{
    if (g_hRealXInput) return;

    char sysPath[MAX_PATH];

    // Prefer xinput1_4.dll and xinput1_3.dll to avoid colliding with this proxy DLL name
    GetSystemDirectoryA(sysPath, MAX_PATH);
    strcat_s(sysPath, "\\xinput1_4.dll");
    g_hRealXInput = LoadLibraryA(sysPath);

    if (!g_hRealXInput)
    {
        GetSystemDirectoryA(sysPath, MAX_PATH);
        strcat_s(sysPath, "\\xinput1_3.dll");
        g_hRealXInput = LoadLibraryA(sysPath);
    }

    if (g_hRealXInput)
    {
        g_pfnGetState = (PFN_XInputGetState)GetProcAddress(g_hRealXInput, "XInputGetState");
        g_pfnSetState = (PFN_XInputSetState)GetProcAddress(g_hRealXInput, "XInputSetState");
        g_pfnGetCaps  = (PFN_XInputGetCapabilities)GetProcAddress(g_hRealXInput, "XInputGetCapabilities");
        g_pfnGetDSound= (PFN_XInputGetDSoundAudioDeviceGuids)GetProcAddress(g_hRealXInput, "XInputGetDSoundAudioDeviceGuids");
    }
}

static int FindDualSenseJoyId()
{
    UINT numDevs = joyGetNumDevs();
    for (UINT i = 0; i < numDevs && i < 16; i++)
    {
        JOYCAPSA jc;
        if (joyGetDevCapsA(i, &jc, sizeof(jc)) == JOYERR_NOERROR)
        {
            if ((jc.wMid == 0x054C && jc.wPid == 0x0CE6) || // DualSense PS5
                (jc.wMid == 0x054C && jc.wPid == 0x0DF2) || // DualSense Edge
                (jc.wMid == 0x054C && jc.wPid == 0x09CC) || // DualShock 4 v2
                (jc.wMid == 0x054C && jc.wPid == 0x05C4) || // DualShock 4 v1
                (jc.wNumAxes >= 4 && jc.wNumButtons >= 10))  // Generic PC Gamepad
            {
                return (int)i;
            }
        }
    }
    return -1;
}

static short ApplyDeadzone(short val, short deadzone)
{
    if (val > -deadzone && val < deadzone) return 0;
    return val;
}

static BOOL ReadDualSenseState(XINPUT_STATE* pState)
{
    int joyId = FindDualSenseJoyId();
    if (joyId < 0) return FALSE;

    JOYINFOEX ji;
    ZeroMemory(&ji, sizeof(ji));
    ji.dwSize = sizeof(ji);
    ji.dwFlags = JOY_RETURNALL;

    if (joyGetPosEx(joyId, &ji) != JOYERR_NOERROR)
        return FALSE;

    g_dwPacketNumber++;
    pState->dwPacketNumber = g_dwPacketNumber;

    XINPUT_GAMEPAD& gp = pState->Gamepad;
    ZeroMemory(&gp, sizeof(gp));

    // Buttons mapping for DualSense
    if (ji.dwButtons & (1 << 0)) gp.wButtons |= XINPUT_GAMEPAD_X; // Square
    if (ji.dwButtons & (1 << 1)) gp.wButtons |= XINPUT_GAMEPAD_A; // Cross
    if (ji.dwButtons & (1 << 2)) gp.wButtons |= XINPUT_GAMEPAD_B; // Circle
    if (ji.dwButtons & (1 << 3)) gp.wButtons |= XINPUT_GAMEPAD_Y; // Triangle

    if (ji.dwButtons & (1 << 4)) gp.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;  // L1
    if (ji.dwButtons & (1 << 5)) gp.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER; // R1

    if (ji.dwButtons & (1 << 8)) gp.wButtons |= XINPUT_GAMEPAD_BACK;  // Create
    if (ji.dwButtons & (1 << 9)) gp.wButtons |= XINPUT_GAMEPAD_START; // Options
    if (ji.dwButtons & (1 << 12)) gp.wButtons |= XINPUT_GAMEPAD_START; // PS
    if (ji.dwButtons & (1 << 13)) gp.wButtons |= XINPUT_GAMEPAD_BACK;  // Touchpad

    if (ji.dwButtons & (1 << 10)) gp.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;  // L3
    if (ji.dwButtons & (1 << 11)) gp.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB; // R3

    // D-Pad from POV Hat
    if (ji.dwPOV != 0xFFFF && ji.dwPOV != 65535)
    {
        DWORD pov = ji.dwPOV;
        if (pov == 0 || pov == 4500 || pov == 31500)
            gp.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
        if (pov == 9000 || pov == 4500 || pov == 13500)
            gp.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        if (pov == 18000 || pov == 13500 || pov == 22500)
            gp.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
        if (pov == 27000 || pov == 22500 || pov == 31500)
            gp.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
    }

    // Left Stick: X (0..65535), Y (0..65535, inverted: 0=up, 65535=down)
    int rawLX = (int)ji.dwXpos - 32768;
    int rawLY = 32767 - (int)ji.dwYpos;
    if (rawLX > 32767) rawLX = 32767;
    if (rawLX < -32768) rawLX = -32768;
    if (rawLY > 32767) rawLY = 32767;
    if (rawLY < -32768) rawLY = -32768;
    gp.sThumbLX = ApplyDeadzone((short)rawLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    gp.sThumbLY = ApplyDeadzone((short)rawLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

    // Right Stick: Z (0..65535), R (0..65535, inverted: 0=up, 65535=down)
    int rawRX = (int)ji.dwZpos - 32768;
    int rawRY = 32767 - (int)ji.dwRpos;
    if (rawRX > 32767) rawRX = 32767;
    if (rawRX < -32768) rawRX = -32768;
    if (rawRY > 32767) rawRY = 32767;
    if (rawRY < -32768) rawRY = -32768;
    gp.sThumbRX = ApplyDeadzone((short)rawRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
    gp.sThumbRY = ApplyDeadzone((short)rawRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

    // Triggers: L2 (Button 6 or U axis centered at 32767), R2 (Button 7 or V axis centered at 32767)
    int leftVal = 0;
    if (ji.dwUpos > 33000)
    {
        leftVal = (int)(((long long)(ji.dwUpos - 32768) * 255) / 32767);
        if (leftVal > 255) leftVal = 255;
        if (leftVal < 0) leftVal = 0;
    }
    if (ji.dwButtons & (1 << 6))
    {
        leftVal = 255;
    }
    gp.bLeftTrigger = (BYTE)leftVal;

    int rightVal = 0;
    if (ji.dwVpos > 33000)
    {
        rightVal = (int)(((long long)(ji.dwVpos - 32768) * 255) / 32767);
        if (rightVal > 255) rightVal = 255;
        if (rightVal < 0) rightVal = 0;
    }
    if (ji.dwButtons & (1 << 7))
    {
        rightVal = 255;
    }
    gp.bRightTrigger = (BYTE)rightVal;

    return TRUE;
}

extern "C"
{
    DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
    {
        InitRealXInput();
        if (!pState) return ERROR_BAD_ARGUMENTS;

        if (dwUserIndex == 0)
        {
            if (g_pfnGetState && g_pfnGetState(0, pState) == ERROR_SUCCESS)
            {
                return ERROR_SUCCESS;
            }

            if (ReadDualSenseState(pState))
            {
                return ERROR_SUCCESS;
            }
            return ERROR_DEVICE_NOT_CONNECTED;
        }

        if (g_pfnGetState)
            return g_pfnGetState(dwUserIndex, pState);

        return ERROR_DEVICE_NOT_CONNECTED;
    }

    DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
    {
        InitRealXInput();
        if (g_pfnSetState)
            return g_pfnSetState(dwUserIndex, pVibration);

        return ERROR_SUCCESS;
    }

    DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities)
    {
        InitRealXInput();
        if (!pCapabilities) return ERROR_BAD_ARGUMENTS;

        if (dwUserIndex == 0)
        {
            if (g_pfnGetCaps && g_pfnGetCaps(0, dwFlags, pCapabilities) == ERROR_SUCCESS)
            {
                return ERROR_SUCCESS;
            }

            if (FindDualSenseJoyId() >= 0)
            {
                ZeroMemory(pCapabilities, sizeof(XINPUT_CAPABILITIES));
                pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
                pCapabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
                pCapabilities->Flags = 0;
                pCapabilities->Gamepad.wButtons = 0xFFFF;
                pCapabilities->Gamepad.bLeftTrigger = 255;
                pCapabilities->Gamepad.bRightTrigger = 255;
                pCapabilities->Gamepad.sThumbLX = 32767;
                pCapabilities->Gamepad.sThumbLY = 32767;
                pCapabilities->Gamepad.sThumbRX = 32767;
                pCapabilities->Gamepad.sThumbRY = 32767;
                pCapabilities->Vibration.wLeftMotorSpeed = 65535;
                pCapabilities->Vibration.wRightMotorSpeed = 65535;
                return ERROR_SUCCESS;
            }
            return ERROR_DEVICE_NOT_CONNECTED;
        }

        if (g_pfnGetCaps)
            return g_pfnGetCaps(dwUserIndex, dwFlags, pCapabilities);

        return ERROR_DEVICE_NOT_CONNECTED;
    }

    DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid)
    {
        InitRealXInput();
        if (g_pfnGetDSound)
            return g_pfnGetDSound(dwUserIndex, pDSoundRenderGuid, pDSoundCaptureGuid);

        return ERROR_NOT_SUPPORTED;
    }

    BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
    {
        if (fdwReason == DLL_PROCESS_ATTACH)
        {
            DisableThreadLibraryCalls(hinstDLL);
        }
        else if (fdwReason == DLL_PROCESS_DETACH)
        {
            if (g_hRealXInput)
            {
                FreeLibrary(g_hRealXInput);
                g_hRealXInput = NULL;
            }
        }
        return TRUE;
    }
}
