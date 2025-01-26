#include <windows.h>
#include "resource.h"
#include "config.h"

/*
 * Currently implemented (names from IDA):
 *  [x] DllMain (export)
 *  [x] Hook (export)
 *  [ ] Unhook (export)
 *  [x] GetDllVersion (export)
 *  [ ] MainHookProc
 *  [x] OtherHookProc
 *  [x] WriteConfigValue
 *  [x] Prob_GetDefaultConfigValue (= GetDefaultConfigValue)
 *  [x] GetConfig
 *  [ ] sub_10001420
 *  [ ] sub_100017C0
 *  [ ] sub_10001910
 *  [x] HandleWheelScroll
 *  [ ] sub_10002810
 *  [ ] sub_100028F0
 *  [ ] sub_10002990
 *  [ ] sub_10002A40
 *  [ ] sub-10002A80
 *  [x] IsTaskSwitcherWindow
 *  [x] FindChildWindow
 *  [ ] sub_10002EB0
 *  [ ] AlmostCertainly_IsExplorerProcess
 *  [ ] IsDesktopWindow
 *  [ ] sub_10003190
 *  [ ] sub_100033C0
 *  [x] FindChildOfParent
 *  [x] FindTaskbarTasksWindow
 *  [ ] ThisIsInteresting
 *  [-] sub_10003C50 (looks like CRT code)
 * 
 * + the 2 resources
 * 
 * = 42.85% reimplemented (12/28 functions, 2/2 resources)
 */

HINSTANCE g_hInst;

// Passed to RegCreateKeyEx class, which is a practice that some programmers did
// in the past. WinAPI documentation has never been clear what that parameter is
// for.
TCHAR EmptyString[] = TEXT("");

// Global text buffer used across the program for getting string values.
TCHAR TextBuffer[512];

int g_dword_1000F01C;

HHOOK g_hhkMain;
HHOOK g_hhkOther;

#define GetClassNameGlobalBuffer(hWnd)         \
    memset(TextBuffer, 0, sizeof(TextBuffer)); \
    GetClassName(hWnd, TextBuffer, 64)         \

#define GetWindowTextGlobalBuffer(hWnd)         \
    memset(TextBuffer, 0, sizeof(TextBuffer));  \
    GetWindowText(hWnd, TextBuffer, 64)         \

bool FindChildWindow(HWND *phWnd, LPCTSTR szClassName, LPCTSTR szWindowText)
{
    HWND hWnd = GetWindow(*phWnd, GW_CHILD);

    if (!hWnd)
    {
        return false;
    }

    while (1)
    {
        GetClassNameGlobalBuffer(hWnd);
        if (lstrcmp(TextBuffer, szClassName) == 0)
        {
            GetWindowTextGlobalBuffer(hWnd);
            if (lstrcmp(TextBuffer, szWindowText) == 0)
            {
                break;
            }
        }

        if (FindChildWindow(&hWnd, szClassName, szWindowText))
        {
            break;
        }

        hWnd = GetWindow(hWnd, GW_HWNDNEXT);
        if (!hWnd)
        {
            return false;
        }
    }

    *phWnd = hWnd;
    return true;
}

// Seemingly always inlined.
inline bool FindParent(HWND *phWnd, LPCTSTR szClassName)
{
    HWND hWndCur = *phWnd;

    if (!*phWnd)
    {
        return false;
    }

    while (1)
    {
        GetClassNameGlobalBuffer(hWndCur);
        if (lstrcmp(TextBuffer, szClassName) == 0)
        {
            break;
        }

        hWndCur = GetParent(hWndCur);

        if (!hWndCur)
        {
            return false;
        }
    }

    *phWnd = hWndCur;
    return true;
}

bool FindChildOfParent(HWND *phWnd, LPCTSTR szParentClassName, LPCTSTR szChildClassName, LPCTSTR szChildWindowText)
{
    HWND hWndCur = *phWnd;

    if (!*phWnd)
    {
        return false;
    }

    FindParent(&hWndCur, szParentClassName);

    if (!hWndCur || !FindChildWindow(&hWndCur, szChildClassName, szChildWindowText))
    {
        return false;
    }

    *phWnd = hWndCur;
    return true;
}

bool FindTaskbarTasksWindow(HWND *phWnd, PDWORD pdwIsMicrosoft)
{
    *pdwIsMicrosoft = 0;

    HWND hWndCur = *phWnd;

    if (*phWnd && FindParent(&hWndCur, TEXT("Shell_TrayWnd")))
    {
        if (hWndCur && FindChildWindow(&hWndCur, TEXT("ToolbarWindow32"), TEXT("Running Applications")))
        {
            *phWnd = hWndCur;
            *pdwIsMicrosoft = 1;
            return true;
        }
    }

    if (GetConfig(TEXT("MultipleMonitors")) == 1 &&
        FindChildOfParent(phWnd, TEXT("UltraMonDeskTaskBar"), TEXT("ToolbarWindow32"), TEXT("")) ||
        FindChildOfParent(phWnd, TEXT("UltraMon Taskbar"), TEXT("ToolbarWindow32"), TEXT("UltraMon Taskbar")))
    {
        return true;
    }

    return false;
}

bool IsTaskSwitcherWindow(HWND hWnd, PDWORD pdwIsMicrosoft)
{
    *pdwIsMicrosoft = 0;

    if (!hWnd)
    {
        return false;
    }

    if (GetConfig(TEXT("MultipleMonitors")) == 1)
    {
        bool fHasTaskbar = false;
        bool fIsUltraMon = false;
        HWND hWndParent = GetParent(hWnd);

        if (hWndParent)
        {
            GetClassNameGlobalBuffer(hWndParent);

            fHasTaskbar = lstrcmp(TextBuffer, TEXT("UltraMon Taskbar")) == 0;
        }

        HWND hWndParent2 = GetParent(hWnd);

        if (hWndParent2)
        {
            HWND hWndParent3 = GetParent(hWndParent2);

            if (hWndParent3)
            {
                HWND hWndParent4 = GetParent(hWndParent3);

                if (hWndParent4)
                {
                    GetClassNameGlobalBuffer(hWndParent4);

                    fIsUltraMon = lstrcmp(TextBuffer, TEXT("UltraMonDeskTaskBar")) == 0;
                }
            }
        }

        if (fHasTaskbar || fIsUltraMon)
        {
            fIsUltraMon = true;
        }

        return *pdwIsMicrosoft || fIsUltraMon;
    }
    else // Microsoft taskbar:
    {
        GetClassNameGlobalBuffer(hWnd);

        if (lstrcmp(TextBuffer, TEXT("ToolbarWindow32")) != 0)
        {
            return false;
        }

        HWND hWndParent = GetParent(hWnd);
        GetClassNameGlobalBuffer(hWndParent);

        if (*pdwIsMicrosoft = lstrcmp(TextBuffer, TEXT("MSTaskSwWClass")) == 0)
        {
            return true;
        }
    }

    return *pdwIsMicrosoft;
}

#include <WindowsX.h>

// XXX(isabella): Needs more work.
LRESULT HandleWheelScroll(WPARAM wParam, LPARAM lParam, HWND hWnd)
{
    // Filter out this message if our settings doesn't allow this:
    if (wParam == WM_MOUSEWHEEL)
    {
        if (GetConfig(TEXT("WheelScroll")) != 1)
        {
            return 0;
        }
    }
    else if (wParam == WM_MBUTTONDOWN)
    {
        if (GetConfig(TEXT("MiddleClickTaskbar")) != 1)
        {
            if (GetConfig(TEXT("MiddleClickTabs")) != 1 ||
                GetConfig(TEXT("MiddleClickTabs_Action")) != 1 ||
                GetAsyncKeyState(VK_CONTROL) < 0)
            {
                return 0;
            }
        }
    }

    POINT point;
    GetCursorPos(&point);
    LPARAM lParam = 0;
    HWND hWndCursor = WindowFromPoint(point);

    DWORD dwIsMicrosoft = 0;
    if (!IsTaskSwitcherWindow(hWndCursor, &dwIsMicrosoft) && !FindTaskbarTasksWindow(&hWndCursor, &dwIsMicrosoft))
    {
        return 0;
    }

    if (wParam == WM_MOUSEWHEEL)
    {
        if (GetAsyncKeyState(VK_LBUTTON) < 0 ||
            GetAsyncKeyState(VK_RBUTTON) < 0 ||
            GetAsyncKeyState(VK_MBUTTON) < 0)
        {
            return 0;
        }

        bool fWheelDirection = GetConfig(TEXT("WheelScroll_Direction")) == 2;

        // probably a bool
        int v9 = 1;

        if (GET_X_LPARAM(lParam) >= 0xF000 && fWheelDirection ||
            GET_X_LPARAM(lParam) < 0xF000 && !fWheelDirection)
        {
            v9 = 0;
        }

        // TODO: what the hell is going on here? probably WM_USER because arguments
        // being passed really don't make sense with TM_GETPOS
        SendMessage(hWnd, 0x400, (WPARAM)hWndCursor, dwIsMicrosoft != 0 | (v9 != 0 ? 2 : 0));
        return 1;
    }

    LRESULT lresSm401 = SendMessage(hWnd, 0x401, (WPARAM)hWndCursor, dwIsMicrosoft != 0);
    if (lresSm401 == -1)
    {
        if (GetConfig(TEXT("MiddleClickTaskbar")) != 1)
        {
            return 0;
        }

        // Possible compiler optimisation? This seems weird...
        int mct = GetConfig(TEXT("MiddleClickTaskbar")) - 1;

        if (mct)
        {
            if (mct == 1)
            {
                HWND hWndDesktop = GetDesktopWindow();
                HWND hWndChild = GetWindow(hWndDesktop, GW_CHILD);

                if (hWndChild)
                {
                    do
                    {
                        if (IsZoomed(hWndChild) && IsWindowVisible(hWndChild))
                        {
                            ShowWindow(hWndChild, SW_RESTORE);
                        }
                        hWndChild = GetWindow(hWndChild, GW_HWNDNEXT);
                    }
                    while (hWndChild);
                    return 1;
                }
            }
        }
        else
        {
            sub_10003190();
        }

        return 1;
    }

    if (!lresSm401 ||
        GetConfig(TEXT("MiddleClickTabs")) != 1 ||
        GetConfig(TEXT("MiddleClickTabs_Action")) != 1 ||
        GetAsyncKeyState(VK_CONTROL) < 0)
    {
        return 0;
    }

    // TODO: research
    SendMessage(hWnd, 0x402, lresSm401, 4);

    return 1;
}

LRESULT OtherHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == 0)
    {
        HWND hWnd = FindWindow(nullptr, TEXT("Taskix_hidden_window"));
        if (hWnd)
        {
            if (HandleWheelScroll(wParam, ((LPCWPRETSTRUCT)lParam)->lParam, hWnd))
            {
                return 1;
            }
        }
    }
}

__declspec(dllexport) EXTERN_C bool Hook(int a1)
{
    if (!g_hhkMain && !g_hhkOther)
    {
        g_dword_1000F01C = a1;
        g_hhkMain = SetWindowsHookEx(7, MainHookProc, g_hInst, 0);

        if (GetConfig(TEXT("_WheelScroll_Method")) != 2)
        {
            return g_hhkMain != nullptr;
        }

        g_hhkOther = SetWindowsHookEx(14, OtherHookProc, g_hInst, 0);

        if (g_hhkMain && g_hhkMain)
        {
            return true;
        }
    }
}

__declspec(dllexport) EXTERN_C UINT GetDllVersion()
{
    return VER_GETDLLVERSION;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID lpvReserved)
{
    g_hInst = hInstance;
    return TRUE;
}