#pragma once
#include <windows.h>

// temporary while I figure out what the type is:
typedef INT_PTR CONFIGRESPONSE;

#define CONFIG_UNSET ( (INT_PTR)-1 )

CONFIGRESPONSE GetConfig(LPCTSTR szName);

CONFIGRESPONSE GetDefaultConfigValue(LPCTSTR szName);

BOOL WriteConfigValue(LPCTSTR szName, DWORD dwValue);