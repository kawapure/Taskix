#include <windows.h>
#include <CommCtrl.h>
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
        int fDraggedUp = 1;

        if (GET_X_LPARAM(lParam) >= 0xF000 && fWheelDirection ||
            GET_X_LPARAM(lParam) < 0xF000 && !fWheelDirection)
        {
            fDraggedUp = 0;
        }

        // TODO: Look into hidden window (implemented in EXE)
        SendMessage(hWndHidden, 0x400, (WPARAM)hWndCursor, dwIsMicrosoft != 0 | (fDraggedUp != 0 ? 2 : 0));
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


struct TaskGroupThingProb
{
    int iFirstButtonId;
    int iSearchButtonId;
    int iLastButtonOffset;
};

bool sub_10002EB0(TaskGroupThingProb *pGroup, HWND hWnd, LONG iButtonId, bool fIsMicrosoft)
{
    pGroup->iSearchButtonId = iButtonId;

    if (fIsMicrosoft)
    {
        int iFirstButtonId = iButtonId;

        if (iButtonId < 0)
        {
            return;
        }

        do
        {
            TBBUTTON tbButton = { 0 };
            SendMessage(hWnd, TB_GETBUTTON, iButtonId, (LPARAM)&tbButton);
            if (tbButton.fsStyle & BTNS_DROPDOWN)
            {
                break;
            }
            --iFirstButtonId;
        }
        while (iFirstButtonId < 0);

        if (iFirstButtonId < 0)
        {
            return false;
        }

        pGroup->iFirstButtonId = iFirstButtonId;

        if (iFirstButtonId < 0)
        {
            return false;
        }

        int iLastButtonId = iButtonId + 1;
        int cButtons = SendMessage(hWnd, TB_BUTTONCOUNT, NULL, NULL);

        if (iButtonId + 1 < cButtons)
        {
            do
            {
                TBBUTTON tbButton = { 0 };
                SendMessage(hWnd, TB_GETBUTTON, iButtonId, (LPARAM)&tbButton);
                if (tbButton.fsState & BTNS_DROPDOWN)
                {
                    break;
                }
                ++iLastButtonId;
            }
            while (iLastButtonId < cButtons);
        }

        pGroup->iLastButtonOffset = iLastButtonId - pGroup->iFirstButtonId;
    }

    return true;
}

int sub_100033C0(HWND hWnd, POINT pt, int *pOut, HWND hWnd2, bool fIsMicrosoft)
{
    if (!IsWindow(hWnd))
    {
        return 0;
    }

    DWORD dwProcessId, dwProcessId2;
    GetWindowThreadProcessId(hWnd, &dwProcessId);
    GetWindowThreadProcessId(hWnd2, &dwProcessId2);

    if (dwProcessId != dwProcessId2)
    {
        return 0;
    }

    GetClassNameGlobalBuffer(hWnd);
    if (lstrcmp(TextBuffer, TEXT("ToolbarWindow32")))
    {
        return 0;
    }

    HWND hWndParent = GetParent(hWnd);
    if (!hWndParent)
    {
        return 0;
    }

    if (!GetClassName(hWndParent, TextBuffer, 64))
    {
        return 0;
    }

    if (lstrcmp(TextBuffer, TEXT("SysPager")) != 0)
    {
        return 0;
    }

    hWndParent = GetParent(hWnd);
    if (!hWndParent)
    {
        return 0;
    }

    if (!GetClassName(hWndParent, TextBuffer, 64))
    {
        return 0;
    }

    if (lstrcmp(TextBuffer, TEXT("MenuSite")) != 0)
    {
        return 0;
    }

    hWndParent = GetParent(hWndParent);
    if (!hWndParent)
    {
        return 0;
    }

    if (!GetClassName(hWndParent, TextBuffer, 64))
    {
        return 0;
    }

    if (lstrcmp(TextBuffer, TEXT("BaseBar")) != 0)
    {
        return 0;
    }

    POINT point = pt;
    ScreenToClient(hWnd, &point);

    int iHit = SendMessage(hWnd, TB_HITTEST, NULL, (LPARAM)&point);
    if (iHit < 0)
    {
        return 0;
    }

    TBBUTTON tbButton = { 0 };
    if (!SendMessageA(hWnd, TB_GETBUTTON, iHit, (LPARAM)&tbButton))
    {
        return 0;
    }

    if (tbButton.idCommand == -1)
    {
        return 0;
    }

    int cButton = SendMessage(hWnd, TB_BUTTONCOUNT, NULL, NULL);
    int iButtonId2 = iHit + tbButton.idCommand - cButton;
    tbButton.iBitmap = 0;
    *pOut = iButtonId2;


}

// Always inlined, from MainHookProc
inline HWND GetTaskbarButtonWindow(TBBUTTON *ptbButton, bool fIsMicrosoft)
{
    return fIsMicrosoft ? *(HWND *)ptbButton->dwData : (HWND)ptbButton->dwData;
}


HWND g_hWnd;
POINT g_ptCurrent;
bool g_fIsMicrosoft;
bool g_fLeftButtonDown;
int g_iHitTestResult;
DWORD dword_1000F028;
DWORD dword_1000F030;
DWORD dword_1000F034;
bool g_fIsDragging;
DWORD dword_1000F04C;
DWORD dword_1000F048;
INT64 qword_1000CC90;
POINT g_ptHit;
HWND hWndTaskTarget;
HCURSOR g_hCursor;
HCURSOR g_hCursorOld;
HWND g_hWndSomething;

LRESULT MainHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    PMOUSEHOOKSTRUCT pRet = (PMOUSEHOOKSTRUCT)lParam;
    LONG y;
    INT64 v44;
    int *v45;
    int *v46;
    POINT point;

    if (code == HC_ACTION)
    {
        HWND hWnd = (HWND)pRet->hwnd;
        POINT pt = pRet->pt;
        HWND hWndHiddenWindow = FindWindow(nullptr, TEXT("Taskix_hidden_window"));

        if (!hWndHiddenWindow)
        {
            return CallNextHookEx(g_hhkMain, code, wParam, lParam);
        }

        y = pt.y;

        if (wParam == WM_LBUTTONDOWN && wParam == WM_LBUTTONUP ||
            GetConfig(TEXT("ShowDesktop")) == 1 ||
            IsDesktopWindow(hWnd))
        {
            bool fDragged = pt.x != g_ptCurrent.x || pt.y != g_ptCurrent.y;

            if (wParam != WM_LBUTTONDOWN || GetFocus() == hWnd)
            {
                g_ptCurrent.x = pt.x;
                g_ptCurrent.y = y;
            }
            else
            {
                g_ptCurrent.x = -1;
            }

            if (wParam == WM_LBUTTONDOWN && GetFocus() != hWnd ||
                wParam == WM_LBUTTONUP && GetFocus() == hWnd && !fDragged)
            {
                v44 = 0;
                LODWORD(v45) = 0;
                point.x = pt.x;
                point.y = y;

                ScreenToClient(hWnd, &point);

                if (SendMessage(hWnd, LVM_HITTEST, NULL, (LPARAM)&point) == -1)
                {
                    SendShowDesktopInputs();
                }
            }
        }

        if (wParam == WM_MBUTTONDOWN && hWnd && g_fIsMicrosoft && 
            GetConfig(TEXT("MiddleClickTabs")) == 1)
        {
            HWND hWnd = WindowFromPoint(pt);
            int v39 = 0;
            HWND hWndAAA = sub_100033C0(hWnd, pt, &v39, hWnd, g_fIsMicrosoft);

            if (hWndAAA)
            {
                if (GetConfig(TEXT("MiddleClickTabs_Action")) == 2 ||
                    GetAsyncKeyState(VK_CONTROL) < 0)
                {
                    PostMessage(hWnd, WM_KEYDOWN, VK_ESCAPE, NULL);
                    PostCloseCommand(hWndAAAA);
                }

                if (GetConfig(TEXT("MiddleClickTabs_Action")) == 1)
                {
                    // Hidden window is implemented in Taskix.exe
                    SendMessage(hWndHiddenWindow, WM_USER + 2, (WPARAM)hWndAAA, 4);
                }
                    
                return CallNextHookEx(g_hhkMain, code, wParam, lParam);
            }
        }
            
        DWORD dwIsMicrosoft;
        bool fIsTaskSwitcherWnd = IsTaskSwitcherWindow(hWnd, &dwIsMicrosoft);
        if (fIsTaskSwitcherWnd)
        {
            g_hWnd = hWnd;
            g_fIsMicrosoft = dwIsMicrosoft;
        }
            
        if (GetConfig(TEXT("_WheelScroll_Method")) == 1 &&
            HandleWheelScroll(wParam, (LPARAM)pRet->hwnd, hWndHiddenWindow))
        {
            return 1;
        }

        HWND hWnd_1 = g_hWnd;

        //if (!fIsTaskSwitcherWnd)
        //{
        //    bool fLeftButtonDown = g_fLeftButtonDown;
        //    if (!g_fLeftButtonDown || wParam != WM_MOUSEMOVE || WindowFromPoint((POINT)pt) != g_hWnd)
        //    {
        //        if (!fLeftButtonDown || wParam != WM_LBUTTONUP)
        //        {
        //            if (fLeftButtonDown)
        //            {
        //                goto LABEL_112;
        //            }
        //            if (wParam != WM_LBUTTONUP)
        //            {
        //                goto LABEL_112;
        //            }
        //            HWND v18 = WindowFromPoint((POINT)pt);
        //            if (v18 != g_hWnd)
        //            {
        //                goto LABEL_112;
        //            }
        //        }
        //    }
        //}

        if (fIsTaskSwitcherWnd ||
            g_fLeftButtonDown && wParam == WM_MOUSEMOVE && WindowFromPoint(pt) == g_hWnd ||
            g_fLeftButtonDown && wParam == WM_LBUTTONUP ||
            !g_fLeftButtonDown && wParam == WM_LBUTTONDOWN && WindowFromPoint(pt) == g_hWnd)
        {
            if (IsOwnProcessWindow(hWnd_1))
            {
                POINT v41 = (POINT)pt;
                ScreenToClient(g_hWnd, &v41);
                LRESULT iHitTestResult = SendMessage(g_hWnd, TB_HITTEST, NULL, (LPARAM)&v41);
                bool v20;
                int v35 = 0;
                HWND v25 = 0;
                if ((int)iHitTestResult >= 0)
                {
                    if (g_fLeftButtonDown &&
                        GetConfig(TEXT("DragTabs")) == 1)
                    {
                        if (wParam == WM_MOUSEMOVE)
                        {
                            int fIsSameWindow = 0;

                            TBBUTTON tbButton = { 0 };
                            if (SendMessage(g_hWnd, TB_GETBUTTON, g_iHitTestResult, (LPARAM)&tbButton))
                            {
                                HWND hWnd = tbButton.fsStyle & BTNS_DROPDOWN
                                    ? nullptr
                                    : GetTaskbarButtonWindow(&tbButton, g_fIsMicrosoft);

                                v25 = hWndTaskTarget;
                                fIsSameWindow = hWndTaskTarget == hWnd;
                            }
                            else
                            {
                                v25 = hWndTaskTarget;
                            }

                            if (!fIsSameWindow)
                            {
                                int v26;
                                if (v25)
                                {
                                    v26 = FindTaskbarButtonForWindow(g_hWnd, v25, g_fIsMicrosoft);
                                }
                                else
                                {
                                    v26 = -1;
                                }

                                if (v26 == -1)
                                {
                                    v20 = 1;
                                    v35 = 1;
                                }
                                else
                                {
                                    g_iHitTestResult = v26;
                                }
                            }

                            if (!v20 && (abs(pt.x - g_ptHit.x) > 5 ||
                                abs(y - g_ptHit.y)) > 5)
                            {
                                BOOL fIsHorizontal = IsWindowRectWide(g_hWnd);
                                ChangeCursor(g_hWnd, fIsHorizontal, &g_hCursor, &g_hCursorOld);
                            }

                            if (iHitTestResult != g_iHitTestResult && !v20)
                            {
                                v20 = sub_10001420(g_hWnd, &g_iHitTestResult, iHitTestResult) == 0;
                                v35 = v20;
                            }
                        }
                        else if (wParam == WM_LBUTTONUP && dword_1000F030)
                        {
                            SendMessage(g_hWnd, WM_MOUSEMOVE, 1, -1);
                            SendMessage(g_hWnd, WM_LBUTTONUP, 0, -1);
                        }
                    }
                    else
                    {
                        if (wParam == WM_LBUTTONDOWN && GetConfig(TEXT("DragTabs")) == 1)
                        {
                            g_fLeftButtonDown = true;
                            g_iHitTestResult = iHitTestResult;
                            g_ptHit = pt;

                            TBBUTTON tbButton = { 0 };
                            if (SendMessage(g_hWnd, TB_GETBUTTON, iHitTestResult, (LPARAM)&tbButton))
                            {
                                HWND hWnd = tbButton.fsStyle & BTNS_DROPDOWN
                                    ? nullptr
                                    : GetTaskbarButtonWindow(&tbButton, g_fIsMicrosoft);

                                hWndTaskTarget = hWnd;
                            }
                            dword_1000F034 = iHitTestResult;
                        }
                        if (wParam == WM_MBUTTONDOWN && GetConfig(TEXT("MiddleClickTabs")) == 1)
                        {
                            TBBUTTON tbButton;
                            if (SendMessage(g_hWnd, TB_GETBUTTON, iHitTestResult, (LPARAM)&tbButton))
                            {
                                HWND hWnd = GetTaskbarButtonWindow(&tbButton, g_fIsMicrosoft);

                                if (GetConfig(TEXT("MiddleClickTabs_Action")) == 2 ||
                                    GetAsyncKeyState(VK_CONTROL) < 0)
                                {
                                    PostCloseCommand(hWnd);
                                }
                            }
                        }
                    }
                }

                if (wParam == WM_LBUTTONUP && g_fLeftButtonDown || v20)
                {
                    g_fLeftButtonDown = false;
                    g_iHitTestResult = -1;
                    dword_1000F028 = 0;
                    dword_1000F030 = 0;

                    RestoreCursor(&g_hCursorOld);
                }

                return CallNextHookEx(g_hhkMain, code, wParam, lParam);
            }
        }

        BOOL v28 = 0;
        BOOL v37 = 0;
        int v40 = 0;

        if (hWnd_1 && hWnd != hWnd_1)
        {
            v28 = sub_100033C0(hWnd, pt, &v40, hWnd_1, g_fIsMicrosoft);
            v37 = v28; // probably useless
        }

        // Different function? The variable use here is peculiar
        HWND v29 = g_hWndSomething;
        if (v28)
        {
            v29 = hWnd;
        }
        g_hWndSomething = v29;

        if (v28 && GetConfig(TEXT("DragTabs")) == 1)
        {
            POINT v36 = pt;
            ScreenToClient(g_hWndSomething, &v36);
            LRESULT v30 = SendMessage(g_hWndSomething, TB_HITTEST, NULL, (LPARAM)&v36);
            if ((int)v30 >= 0)
            {
                if (g_fIsDragging)
                {
                    if (wParam == WM_MOUSEMOVE)
                    {
                        if (abs(y - HIDWORD(qword_1000CC90)) > 5)
                        {
                            ChangeCursor(hWnd, 0, &g_hCursor, g_hCursorOld);
                        }

                        if (v30 != dword_1000F048)
                        {
                            sub_100017C0(g_hWndSomething, &dword_1000F048, v30);
                        }
                    }
                }
                else if (wParam == WM_MOUSEMOVE &&
                    GetAsyncKeyState(VK_LBUTTON) < 0 &&
                    GetAsyncKeyState(VK_RBUTTON) < 0)
                {
                    g_fIsDragging = true;
                    dword_1000F048 = v30;
                    qword_1000CC90 = pt;
                }
            }

            if (wParam == WM_LBUTTONUP)
            {
                if (g_fIsDragging)
                {
                    int v31 = dword_1000F04C;
                    g_fIsDragging = false;
                    dword_1000F048 = -1;
                    dword_1000F04C = 0;

                    RestoreCursor(g_hCursorOld);

                    if (v31)
                    {
                        SendMessage(g_hWndSomething, WM_MOUSEMOVE, 1, -1);
                        SendMessage(g_hWndSomething, WM_LBUTTONUP, 0, -1);
                        return 1;
                    }
                }
            }
        }
    }

    return CallNextHookEx(g_hhkMain, code, wParam, lParam);
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