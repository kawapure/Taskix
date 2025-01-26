#include "config.h"

CONFIGRESPONSE GetConfig(LPCTSTR szName)
{
    HKEY hKey = nullptr;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Robust IT\\Taskix"), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        // Write default config if the key cannot be opened:
        WriteConfigValue(szName, GetDefaultConfigValue(szName));
    }

    DWORD dwType = REG_NONE;
    DWORD dwData = 0;
    DWORD cbData = sizeof(DWORD);
    
    LSTATUS lResult = RegQueryValueEx(hKey, szName, nullptr, &dwType, (LPBYTE)&dwData, &cbData);

    RegCloseKey(hKey);

    if (lResult != ERROR_SUCCESS)
    {
        // Write default value if the value still cannot be opened:
        dwData = GetDefaultConfigValue(szName);
        WriteConfigValue(szName, dwData);
    }

    return dwData;
}

CONFIGRESPONSE GetDefaultConfigValue(LPCTSTR szName)
{
    if (
        !lstrcmp(szName, TEXT("_WheelScrollMethod")) ||
        !lstrcmp(szName, TEXT("_ShowDesktop_Method")) ||
        !lstrcmp(szName, TEXT("_ShowDesktop_Type"))
    )
    {
        return 2;
    }

    if (
        !lstrcmp(szName, TEXT("ChangeCursor")) ||
        !lstrcmp(szName, TEXT("DragTabs"))
        )
    {
        return 1;
    }

    if (
        !lstrcmp(szName, TEXT("MiddleClickTabs"))
    )
    {
        return 0;
    }

    if (
        !lstrcmp(szName, TEXT("MiddleClickTabs_Action"))
    )
    {
        return 1;
    }

    if (
        !lstrcmp(szName, TEXT("MiddleClickTaskbar"))
    )
    {
        return 0;
    }

    if (
        !lstrcmp(szName, TEXT("MiddleClickTaskbar_Action"))
    )
    {
        return 1;
    }

    if (
        !lstrcmp(szName, TEXT("MultipleMonitors")) ||
        !lstrcmp(szName, TEXT("ShowDesktop")) ||
        !lstrcmp(szName, TEXT("WheelScroll"))
    )
    {
        return 0;
    }

    if (
        !lstrcmp(szName, TEXT("WheelScroll_Direction"))
    )
    {
        return 1;
    }

    if (
        !lstrcmp(szName, TEXT("WheelScroll_SkipMinimized"))
    )
    {
        return 0;
    }

    if (
        !lstrcmp(szName, TEXT("WheelScroll_Wrap"))
    )
    {
        return 0;
    }

    return -1;
}

BOOL WriteConfigValue(LPCTSTR szName, DWORD dwValue)
{
    HKEY hkSoftware = nullptr;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software"), 0, KEY_CREATE_SUB_KEY, &hkSoftware) != ERROR_SUCCESS)
    {
        return FALSE;
    }

    DWORD dwDispositionRobustIT = 0;
    HKEY hkRobustIT = nullptr;
    LSTATUS lsRobustIT = RegCreateKeyEx(
        hkSoftware,
        TEXT("Robust IT"),
        0,
        EmptyString,
        0,
        KEY_ALL_ACCESS,
        nullptr,
        &hkRobustIT,
        &dwDispositionRobustIT);
    RegCloseKey(hkSoftware);
    if (lsRobustIT != ERROR_SUCCESS)
    {
        return FALSE;
    }

    DWORD dwDispositionTaskix = 0;
    HKEY hkTaskix = nullptr;
    LSTATUS lsTaskix = RegCreateKeyEx(
        hkSoftware,
        TEXT("Taskix"),
        0,
        EmptyString,
        0,
        KEY_ALL_ACCESS,
        nullptr,
        &hkTaskix,
        &dwDispositionTaskix);
    RegCloseKey(hkRobustIT);
    if (lsTaskix != ERROR_SUCCESS)
    {
        return FALSE;
    }

    LRESULT lsSetValue = RegSetValueEx(
        hkTaskix, szName, 0, KEY_CREATE_SUB_KEY, (LPBYTE)&dwValue, sizeof(DWORD)
    );
    RegCloseKey(hkTaskix);

    return lsSetValue == ERROR_SUCCESS;
}