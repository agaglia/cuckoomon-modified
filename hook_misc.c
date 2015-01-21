/*
Cuckoo Sandbox - Automated Malware Analysis
Copyright (C) 2010-2014 Cuckoo Sandbox Developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include "ntapi.h"
#include "hooking.h"
#include "log.h"
#include "pipe.h"
#include "misc.h"
#include "hook_file.h"
#include "hook_sleep.h"

HOOKDEF(HHOOK, WINAPI, SetWindowsHookExA,
    __in  int idHook,
    __in  HOOKPROC lpfn,
    __in  HINSTANCE hMod,
    __in  DWORD dwThreadId
) {

    HHOOK ret = Old_SetWindowsHookExA(idHook, lpfn, hMod, dwThreadId);
    LOQ_nonnull("system", "ippi", "HookIdentifier", idHook, "ProcedureAddress", lpfn,
        "ModuleAddress", hMod, "ThreadId", dwThreadId);
    return ret;
}

HOOKDEF(HHOOK, WINAPI, SetWindowsHookExW,
    __in  int idHook,
    __in  HOOKPROC lpfn,
    __in  HINSTANCE hMod,
    __in  DWORD dwThreadId
) {

    HHOOK ret = Old_SetWindowsHookExW(idHook, lpfn, hMod, dwThreadId);
    LOQ_nonnull("system", "ippi", "HookIdentifier", idHook, "ProcedureAddress", lpfn,
        "ModuleAddress", hMod, "ThreadId", dwThreadId);
    return ret;
}

HOOKDEF(BOOL, WINAPI, UnhookWindowsHookEx,
    __in  HHOOK hhk
) {

    BOOL ret = Old_UnhookWindowsHookEx(hhk);
    LOQ_bool("hooking", "p", "HookHandle", hhk);
    return ret;
}

HOOKDEF(LPTOP_LEVEL_EXCEPTION_FILTER, WINAPI, SetUnhandledExceptionFilter,
    _In_  LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter
) {
    BOOL ret = 1;
    LPTOP_LEVEL_EXCEPTION_FILTER res;

#if REPORT_EXCEPTIONS
	res = NULL;
#else
	res = Old_SetUnhandledExceptionFilter(lpTopLevelExceptionFilter);
#endif
	LOQ_bool("hooking", "");
    return res;
}

HOOKDEF(UINT, WINAPI, SetErrorMode,
	_In_ UINT uMode
) {
	UINT ret = 0;
#ifndef REPORT_EXCEPTIONS
	ret = Old_SetErrorMode(uMode);
#endif
	LOQ_void("system", "h", "Mode", uMode);
	return ret;
}


HOOKDEF(NTSTATUS, WINAPI, LdrLoadDll,
    __in_opt    PWCHAR PathToFile,
    __in_opt    ULONG Flags,
    __in        PUNICODE_STRING ModuleFileName,
    __out       PHANDLE ModuleHandle
) {
    COPY_UNICODE_STRING(library, ModuleFileName);

    NTSTATUS ret = Old_LdrLoadDll(PathToFile, Flags, ModuleFileName,
        ModuleHandle);
    LOQ_ntstatus("system", "hoP", "Flags", Flags, "FileName", &library,
        "BaseAddress", ModuleHandle);
    return ret;
}

// Called with the loader lock held
HOOKDEF(NTSTATUS, WINAPI, LdrGetDllHandle,
    __in_opt    PWORD pwPath,
    __in_opt    PVOID Unused,
    __in        PUNICODE_STRING ModuleFileName,
    __out       PHANDLE pHModule
) {
    NTSTATUS ret = Old_LdrGetDllHandle(pwPath, Unused, ModuleFileName,
        pHModule);
    LOQ_ntstatus("system", "oP", "FileName", ModuleFileName, "ModuleHandle", pHModule);
    return ret;
}

HOOKDEF(NTSTATUS, WINAPI, LdrGetProcedureAddress,
    __in        HMODULE ModuleHandle,
    __in_opt    PANSI_STRING FunctionName,
    __in_opt    WORD Ordinal,
    __out       PVOID *FunctionAddress
) {
    NTSTATUS ret = Old_LdrGetProcedureAddress(ModuleHandle, FunctionName,
        Ordinal, FunctionAddress);
    LOQ_ntstatus("system", "pSiP", "ModuleHandle", ModuleHandle,
        "FunctionName", FunctionName != NULL ? FunctionName->Length : 0,
            FunctionName != NULL ? FunctionName->Buffer : NULL,
        "Ordinal", Ordinal, "FunctionAddress", FunctionAddress);
    return ret;
}

HOOKDEF(BOOL, WINAPI, DeviceIoControl,
    __in         HANDLE hDevice,
    __in         DWORD dwIoControlCode,
    __in_opt     LPVOID lpInBuffer,
    __in         DWORD nInBufferSize,
    __out_opt    LPVOID lpOutBuffer,
    __in         DWORD nOutBufferSize,
    __out_opt    LPDWORD lpBytesReturned,
    __inout_opt  LPOVERLAPPED lpOverlapped
) {
	BOOL ret = Old_DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer,
		nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned,
		lpOverlapped);
    LOQ_bool("device", "phbb", "DeviceHandle", hDevice, "IoControlCode", dwIoControlCode,
        "InBuffer", nInBufferSize, lpInBuffer,
        "OutBuffer", lpBytesReturned ? *lpBytesReturned : nOutBufferSize,
            lpOutBuffer);

	/* Fake harddrive size to 256GB */
	if (lpOutBuffer && nOutBufferSize >= sizeof(GET_LENGTH_INFORMATION) && dwIoControlCode == IOCTL_DISK_GET_LENGTH_INFO) {
		((PGET_LENGTH_INFORMATION)lpOutBuffer)->Length.QuadPart = 256060514304L;
	}

	return ret;
}

HOOKDEF(BOOL, WINAPI, ExitWindowsEx,
    __in  UINT uFlags,
    __in  DWORD dwReason
) {
    BOOL ret = 0;
    LOQ_bool("system", "hi", "Flags", uFlags, "Reason", dwReason);
    return Old_ExitWindowsEx(uFlags, dwReason);
}

static int num_isdebuggerpresent;

HOOKDEF(BOOL, WINAPI, IsDebuggerPresent,
    void
) {

    BOOL ret = Old_IsDebuggerPresent();
	num_isdebuggerpresent++;
	if (num_isdebuggerpresent < 20)
		LOQ_bool("system", "");
	else if (num_isdebuggerpresent == 20)
		LOQ_bool("system", "s", "Status", "Log limit reached");
	return ret;
}

HOOKDEF(BOOL, WINAPI, LookupPrivilegeValueW,
    __in_opt  LPWSTR lpSystemName,
    __in      LPWSTR lpName,
    __out     PLUID lpLuid
) {

    BOOL ret = Old_LookupPrivilegeValueW(lpSystemName, lpName, lpLuid);
    LOQ_bool("system", "uu", "SystemName", lpSystemName, "PrivilegeName", lpName);
    return ret;
}

HOOKDEF(NTSTATUS, WINAPI, NtClose,
    __in    HANDLE Handle
) {
    NTSTATUS ret = Old_NtClose(Handle);
    LOQ_ntstatus("system", "p", "Handle", Handle);
    if(NT_SUCCESS(ret)) {
        file_close(Handle);
    }
    return ret;
}

HOOKDEF(BOOL, WINAPI, WriteConsoleA,
    _In_        HANDLE hConsoleOutput,
    _In_        const VOID *lpBuffer,
    _In_        DWORD nNumberOfCharsToWrite,
    _Out_       LPDWORD lpNumberOfCharsWritten,
    _Reserved_  LPVOID lpReseverd
) {
    BOOL ret = Old_WriteConsoleA(hConsoleOutput, lpBuffer,
        nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReseverd);
    LOQ_bool("system", "pS", "ConsoleHandle", hConsoleOutput,
        "Buffer", nNumberOfCharsToWrite, lpBuffer);
    return ret;
}

HOOKDEF(BOOL, WINAPI, WriteConsoleW,
    _In_        HANDLE hConsoleOutput,
    _In_        const VOID *lpBuffer,
    _In_        DWORD nNumberOfCharsToWrite,
    _Out_       LPDWORD lpNumberOfCharsWritten,
    _Reserved_  LPVOID lpReseverd
) {
    BOOL ret = Old_WriteConsoleW(hConsoleOutput, lpBuffer,
        nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReseverd);
    LOQ_bool("system", "pU", "ConsoleHandle", hConsoleOutput,
        "Buffer", nNumberOfCharsToWrite, lpBuffer);
    return ret;
}

HOOKDEF(NTSTATUS, WINAPI, NtMapViewOfSection,
    _In_     HANDLE SectionHandle,
    _In_     HANDLE ProcessHandle,
    __inout  PVOID *BaseAddress,
    _In_     ULONG_PTR ZeroBits,
    _In_     SIZE_T CommitSize,
    __inout  PLARGE_INTEGER SectionOffset,
    __inout  PSIZE_T ViewSize,
    __in     UINT InheritDisposition,
    __in     ULONG AllocationType,
    __in     ULONG Win32Protect
) {
    NTSTATUS ret = Old_NtMapViewOfSection(SectionHandle, ProcessHandle,
        BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize,
        InheritDisposition, AllocationType, Win32Protect);
    LOQ_ntstatus("process", "ppPpPh", "SectionHandle", SectionHandle,
        "ProcessHandle", ProcessHandle, "BaseAddress", BaseAddress,
        "SectionOffset", SectionOffset, "ViewSize", ViewSize, "Win32Protect", Win32Protect);

    if(NT_SUCCESS(ret)) {
		DWORD pid = pid_from_process_handle(ProcessHandle);
        pipe("PROCESS:%d:%d", is_suspended(pid, 0), pid);
        disable_sleep_skip();
    }
    return ret;
}

HOOKDEF(int, WINAPI, GetSystemMetrics,
    _In_  int nIndex
) {
    int ret = Old_GetSystemMetrics(nIndex);
    LOQ_nonzero("misc", "i", "SystemMetricIndex", nIndex);
    return ret;
}

HOOKDEF(BOOL, WINAPI, GetCursorPos,
    _Out_ LPPOINT lpPoint
) {
    BOOL ret = Old_GetCursorPos(lpPoint);
	static LARGE_INTEGER last_skipped = { .QuadPart = 0ULL };

	/* work around the fact that skipping sleeps prevents the human module from making the system look active */
	if (lpPoint && time_skipped.QuadPart != last_skipped.QuadPart) {
		int xres, yres;
		xres = GetSystemMetrics(0);
		yres = GetSystemMetrics(1);
		lpPoint->x = random() % xres;
		lpPoint->y = random() % yres;
		last_skipped.QuadPart = time_skipped.QuadPart;
	}
	else if (last_skipped.QuadPart == 0) {
		last_skipped.QuadPart = time_skipped.QuadPart;
	}

	LOQ_bool("misc", "ii", "x", lpPoint != NULL ? lpPoint->x : 0,
        "y", lpPoint != NULL ? lpPoint->y : 0);
	
	return ret;
}

HOOKDEF(DWORD, WINAPI, GetLastError,
	void
)
{
	DWORD ret = Old_GetLastError();
	LOQ_void("misc", "");
	return ret;
}

HOOKDEF(BOOL, WINAPI, GetComputerNameA,
    _Out_    LPSTR lpBuffer,
    _Inout_  LPDWORD lpnSize
) {
    BOOL ret = Old_GetComputerNameA(lpBuffer, lpnSize);
    LOQ_bool("misc", "s", "ComputerName", lpBuffer);
    return ret;
}

HOOKDEF(BOOL, WINAPI, GetComputerNameW,
    _Out_    LPWSTR lpBuffer,
    _Inout_  LPDWORD lpnSize
) {
    BOOL ret = Old_GetComputerNameW(lpBuffer, lpnSize);
    LOQ_bool("misc", "u", "ComputerName", lpBuffer);
    return ret;
}

HOOKDEF(BOOL, WINAPI, GetUserNameA,
	_Out_    LPSTR lpBuffer,
    _Inout_  LPDWORD lpnSize
) {
    BOOL ret = Old_GetUserNameA(lpBuffer, lpnSize);
    LOQ_bool("misc", "s", "Name", lpBuffer);
    return ret;
}

HOOKDEF(BOOL, WINAPI, GetUserNameW,
	_Out_    LPWSTR lpBuffer,
    _Inout_  LPDWORD lpnSize
) {
    BOOL ret = Old_GetUserNameW(lpBuffer, lpnSize);
    LOQ_bool("misc", "u", "Name", lpBuffer);
    return ret;
}
