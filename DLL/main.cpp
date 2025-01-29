#include <windows.h>
#include "resource.h"
#include "config.h"

/*
 * Currently implemented (names from IDA):
 *  [x] DllMain (export)
 *  [x] Hook (export)
 *  [x] Unhook (export)
 *  [x] GetDllVersion (export)
 *  [ ] MainHookProc
 *  [x] OtherHookProc
 *  [x] WriteConfigValue
 *  [x] Prob_GetDefaultConfigValue (= GetDefaultConfigValue)
 *  [x] GetConfig
 *  [ ] sub_10001420
 *  [ ] sub_100017C0
 *  [x] PostCloseCommand
 *  [x] HandleWheelScroll
 *  [x] FindTaskbarButtonForWindow
 *  [x] SimulateClickCenter
 *  [x] ChangeCursor
 *  [x] RestoreCursor
 *  [x] IsWindowRectWide
 *  [x] IsTaskSwitcherWindow
 *  [x] FindChildWindow
 *  [ ] sub_10002EB0
 *  [x] IsOwnProcessWindow
 *  [x] IsDesktopWindow
 *  [x] SendShowDesktopInputs
 *  [ ] sub_100033C0
 *  [x] FindChildOfParent
 *  [x] FindTaskbarTasksWindow
 * 
 * + the 2 resources
 * 
 * = 81.48% reimplemented (22/27 functions, 2/2 resources)
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
    GetClassName(hWnd, TextBuffer, 64)         

#define GetWindowTextGlobalBuffer(hWnd)         \
    memset(TextBuffer, 0, sizeof(TextBuffer));  \
    GetWindowText(hWnd, TextBuffer, 64)         

bool IsOwnProcessWindow(HWND hWnd)
{
    DWORD dwProcessId;
    GetWindowThreadProcessId(hWnd, &dwProcessId);
    return GetCurrentProcessId() == dwProcessId;
}

bool IsDesktopWindow(HWND hWnd)
{
    if (hWnd)
    {
        GetClassNameGlobalBuffer(hWnd);
        if (lstrcmp(TextBuffer, TEXT("SysListView32")) == 0)
        {
            HWND hWndParent = GetParent(hWnd);
            GetClassNameGlobalBuffer(hWndParent);
            if (lstrcmp(TextBuffer, TEXT("SHELLDLL_DefView")) == 0)
            {
                HWND hWndParent = GetParent(hWnd);
                GetClassNameGlobalBuffer(hWndParent);
                return lstrcmp(TextBuffer, TEXT("Progman")) == 0;
            }
        }
    }

    return false;
}

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
LRESULT HandleWheelScroll(WPARAM wParam, LPARAM lParam, HWND hWndHidden)
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

        // TODO: Look into hidden window (implemented in EXE)
        SendMessage(hWndHidden, 0x400, (WPARAM)hWndCursor, dwIsMicrosoft != 0 | (v9 != 0 ? 2 : 0));
        return 1;
    }

    LRESULT lresSm401 = SendMessage(hWndHidden, 0x401, (WPARAM)hWndCursor, dwIsMicrosoft != 0);
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
            SendShowDesktopInputs();
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
    SendMessage(hWndHidden, 0x402, lresSm401, 4);

    return 1;
}

inline HWND FirstNearestVisibleTopLevelWindow(HWND hWnd, DWORD dwProcessIdSearch)
{
    RECT rc;
    HWND hWndCur;

    for (hWndCur = hWnd; hWndCur; hWndCur = GetWindow(hWndCur, GW_HWNDNEXT))
    {
        if (IsWindowVisible(hWndCur))
        {
            GetClientRect(hWndCur, &rc);
            if (!IsRectEmpty(&rc))
            {
                DWORD dwProcessId = 0;
                GetWindowThreadProcessId(hWndCur, &dwProcessId);
                if (dwProcessId != dwProcessIdSearch ||
                    (GetWindowLong(hWndCur, GWL_STYLE) & WS_CHILD | WS_POPUP == 0) &&
                    (GetWindowLong(hWndCur, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) == 0)
                {
                    break;
                }
            }
        }
    }

    return hWndCur;
}

void SendShowDesktopInputs()
{
    if (GetConfig(TEXT("_ShowDesktop_Type")) == 2)
    {
        DWORD dwProcessId = 0;
        HWND hWndTray = FindWindow(TEXT("Shell_TrayWnd"), nullptr);

        if (hWndTray)
        {
            GetWindowThreadProcessId(hWndTray, &dwProcessId);
        }

        HWND hWndDesktop = GetDesktopWindow();
        HWND hWndDesktopChild = GetWindow(hWndDesktop, GW_CHILD);

        if (!hWndDesktopChild)
        {
            return;
        }

        if (!FirstNearestVisibleTopLevelWindow(hWndDesktopChild, dwProcessId))
        {
            return;
        }
    }

    if (GetAsyncKeyState(VK_SHIFT) >= 0 &&
        GetAsyncKeyState(VK_CONTROL) >= 0 &&
        GetAsyncKeyState(VK_MENU) >= 0)
    {
        INPUT inputs[4] = {
            {INPUT_KEYBOARD},
            {INPUT_KEYBOARD},
            {INPUT_KEYBOARD},
            {INPUT_KEYBOARD}
        };

        if (GetConfig(TEXT("_ShowDesktop_Method")))
        {
            inputs[0].ki.wScan = VK_LWIN;
            inputs[0].mi.dy = 9;

            inputs[1].ki.wScan = VK_SPACE;
            inputs[1].mi.dy = 8;

            inputs[2].ki.wScan = VK_SPACE;
            inputs[2].mi.dy = 10;

            inputs[3].ki.wScan = VK_LWIN;
            inputs[3].mi.dy = 11;
        }
        else
        {
            inputs[0].ki.wVk = VK_LWIN;

            inputs[1].ki.wVk = 'D';

            inputs[2].ki.wVk = 'D';
            inputs[2].mi.dy = 2;

            inputs[3].ki.wVk = VK_LWIN;
            inputs[3].mi.dy = 2;
        }

        SendInput(ARRAYSIZE(inputs), inputs, sizeof(INPUT));
    }
}

bool IsWindowRectWide(HWND hWnd)
{
    RECT rc;
    GetWindowRect(hWnd, &rc);
    return rc.right - rc.left > rc.bottom - rc.top;
}

bool PostCloseCommand(HWND hWnd)
{
    if (!IsWindow(hWnd) || !IsWindowEnabled(hWnd))
    {
        return false;
    }

    PostMessage(hWnd, WM_SYSCOMMAND, SC_CLOSE, NULL);
    return true;
}

bool ChangeCursor(HWND hWnd, bool fHorizontal, HCURSOR *phCursor, HCURSOR *phCursorOld)
{
    if (GetConfig(TEXT("ChangeCursor")) == 1)
    {
        HMODULE hm = GetModuleHandle(TEXT(VER_FILENAME_STR));

        HCURSOR hCur = LoadCursor(
            hm, 
            MAKEINTRESOURCE(fHorizontal ? IDC_DRAGHORIZONTAL : IDC_DRAGVERTICAL)
        );

        *phCursor = hCur;

        if (hCur)
        {
            HCURSOR hCurOld = GetCursor();
            if (hCurOld != *phCursor)
            {
                *phCursorOld = hCurOld;
            }

            SetCursor(*phCursor);

            if (fHorizontal)
            {
                return SetCapture(hWnd);
            }
        }

        return true;
    }

    return false;
}

bool RestoreCursor(HCURSOR *phCursorOrig)
{
    bool fResult;

    if ((fResult = GetConfig(TEXT("ChangeCursor"))) == 1)
    {
        if (*phCursorOrig)
        {
            SetCursor(*phCursorOrig);
            fResult = ReleaseCapture();
            *phCursorOrig = nullptr;
        }
    }

    return fResult;
}

LRESULT SimulateClickCenter(HWND hWnd, int iButtonId)
{
    RECT rect;
    LRESULT lr = SendMessage(hWnd, TB_GETRECT, iButtonId, (LPARAM)&rect);

    if (lr)
    {
        int iCenterX = (rect.right + rect.left) / 2;
        int iCenterY = (rect.top + rect.bottom) / 2;

        SendMessage(hWnd, WM_MOUSEMOVE, MK_LBUTTON, -1);
        SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, iCenterX | (iCenterY << 16));

        return 1;
    }

    return lr;
}

int FindTaskbarButtonForWindow(HWND hWndTaskbar, HWND hWndButtonTarget, int fIsMicrosoft)
{
    int cButtons = SendMessage(hWndTaskbar, TB_BUTTONCOUNT, NULL, NULL);

    if (cButtons <= 0)
    {
        return -1;
    }

    for (int i = 0; i < cButtons; i++)
    {
        TBBUTTON tbButton = { 0 };

        if (SendMessage(hWndTaskbar, TB_GETBUTTON, i, (LPARAM)&tbButton))
        {
            HWND hWnd = GetTaskbarButtonWindow(&tbButton, fIsMicrosoft);

            if (hWnd == hWndButtonTarget)
            {
                return i;
            }
        }
    }

    return -1;
}

// Always inlined, from MainHookProc
inline HWND GetTaskbarButtonWindow(TBBUTTON *ptbButton, bool fIsMicrosoft)
{
    return fIsMicrosoft ? *(HWND *)ptbButton->dwData : (HWND)ptbButton->dwData;
}

LRESULT OtherHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION)
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

    // g_hhkMain use here is not a mistake; original implementation does that too.
    return CallNextHookEx(g_hhkMain, code, wParam, lParam);
}

__declspec(dllexport) EXTERN_C bool Hook(int a1)
{
    if (!g_hhkMain && !g_hhkOther)
    {
        g_dword_1000F01C = a1;
        g_hhkMain = SetWindowsHookEx(WH_MOUSE, MainHookProc, g_hInst, 0);

        if (GetConfig(TEXT("_WheelScroll_Method")) != 2)
        {
            return g_hhkMain != nullptr;
        }

        g_hhkOther = SetWindowsHookEx(WH_MOUSE_LL, OtherHookProc, g_hInst, 0);

        if (g_hhkMain && g_hhkMain)
        {
            return true;
        }
    }
}

__declspec(dllexport) EXTERN_C bool Unhook()
{
    UnhookWindowsHookEx(g_hhkMain);
    UnhookWindowsHookEx(g_hhkOther);
    g_hhkMain = nullptr;
    g_hhkOther = nullptr;

    return true;
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