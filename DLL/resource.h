﻿#include <windows.h>

#if _WIN64
#define VER_FILENAME_STR "Taskix64.dll"
#else
#define VER_FILENAME_STR "Taskix32.dll"
#endif

#define VER_MAJOR 2
#define VER_MINOR 1
#define VER_PATCH 1

#define VER_FILEVERSION     2,1,1,0
#define VER_FILEVERSION_STR "2.1"
#define VER_GETDLLVERSION 20101
#define VER_PRODUCTVERSION VER_FILEVERSION
#define VER_PRODUCTVERSION_STR VER_FILEVERSION_STR

#define VER_COMPANYNAME_STR    "Robust IT"
#define VER_LEGALCOPYRIGHT_STR "Copyright © 2009 Robust IT"
#define VER_PRODUCTNAME_STR    "Taskix"

#define IDC_DRAGHORIZONTAL  101
#define IDC_DRAGVERTICAL    102
