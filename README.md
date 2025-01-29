# Taskix

**Taskix** was a utility created by Adrian Schlesigner (Robust IT) in 2006 for improving the functionality of the Windows taskbar. It supported Windows XP and Windows Vista, but was abandoned after the release of Windows 7, receiving its final update (version 2.1) on the 5th of November 2009.

Taskix was a useful utility for being able to reorder taskbar items on Windows XP and Windows Vista, a feature not natively offered until Windows 7. Additionally, it allowed other mouse shortcuts to be configured on the taskbar, in a manner similarly applicable today with 7+ Taskbar Tweaker or Windhawk.

With its last update being in 2009, and the homepage at which to download it having went down sometime between 2019 and 2021, this program is now completely abandonware. I took an interest in it, so I wanted to reverse engineer it to figure out how it pulled off its functionality in the Windows XP taskbar. I began this project as part of research for [ExplorerEx](//github.com/kfh83/ExplorerEx).

## Progress

`Taskix{32, 64}.dll` (main DLL) is currently **77.8%** complete, featuring the following functions:
 - [x] `DllMain` (export)
 - [x] `Hook` (export)
 - [x] `Unhook` (export)
 - [x] `GetDllVersion` (export)
 - [ ] `MainHookProc`
 - [x] `OtherHookProc`
 - [x] `WriteConfigValue`
 - [x] `Prob_GetDefaultConfigValue` (= GetDefaultConfigValue)
 - [x] `GetConfig`
 - [ ] `sub_10001420`
 - [ ] `sub_100017C0`
 - [x] `PostCloseCommand`
 - [x] `HandleWheelScroll`
 - [ ] `sub_10002810`
 - [x] `SimulateClickCenter`
 - [x] `ChangeCursor`
 - [x] `RestoreCursor`
 - [x] `IsWindowRectWide`
 - [x] `IsTaskSwitcherWindow`
 - [x] `FindChildWindow`
 - [ ] `sub_10002EB0`
 - [x] `IsOwnProcessWindow`
 - [x] `IsDesktopWindow`
 - [x] `SendShowDesktopInputs`
 - [ ] `sub_100033C0`
 - [x] `FindChildOfParent`
 - [x] `FindTaskbarTasksWindow`

`Taskix{32, 64}.exe` (mostly configuration GUI) is currently **0%** complete. That is to say, development has not even begun on it yet.