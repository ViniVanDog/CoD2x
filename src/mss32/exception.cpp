#include "exception.h"

#include <windows.h>
#include <stdio.h>
#include <dbghelp.h>

#include "shared.h"
#include "../shared/cod2_shared.h"
#include "../shared/cod2_net.h"

volatile bool exception_processCrashed = false;
extern struct netaddr_s updater_address;


/*
 * Determine the exception name based on the exception code
 */
const char* exception_getText(DWORD exceptionCode) {
    const char *exceptionName;
    switch (exceptionCode) {
        case EXCEPTION_ACCESS_VIOLATION:
            exceptionName = "ACCESS_VIOLATION";
            break;
        case EXCEPTION_STACK_OVERFLOW:
            exceptionName = "EXCEPTION_STACK_OVERFLOW";
            break;
        case EXCEPTION_FLT_DIVIDE_BY_ZERO:
            exceptionName = "FLT_DIVIDE_BY_ZERO";
            break;
        case EXCEPTION_INT_DIVIDE_BY_ZERO:
            exceptionName = "INT_DIVIDE_BY_ZERO";
            break;
        case EXCEPTION_ILLEGAL_INSTRUCTION:
            exceptionName = "ILLEGAL_INSTRUCTION";
            break;
        case EXCEPTION_PRIV_INSTRUCTION:
            exceptionName = "PRIV_INSTRUCTION";
            break;
        default:
            exceptionName = "Unknown Exception";
            break;
    }
    return exceptionName;
}



bool exception_createMiniDump(EXCEPTION_POINTERS* pExceptionInfo) {
    HANDLE hFile = CreateFile("CoD2MP_s.crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBox(NULL, "Failed to create crash dump file.", "Application error", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION mdei;
    mdei.ThreadId = GetCurrentThreadId();
    mdei.ExceptionPointers = pExceptionInfo;
    mdei.ClientPointers = FALSE;

    BOOL bSuccess = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                      hFile, MiniDumpWithFullMemory,
                                      (pExceptionInfo ? &mdei : NULL), NULL, NULL);
    CloseHandle(hFile);

    if (!bSuccess) {
        MessageBox(NULL, "Failed to create crash dump file.", "Application error", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return false;
    }

    return true;
}



/** Called when exception happend */
LONG WINAPI exception_handler(EXCEPTION_POINTERS* pExceptionInfo) {

    // In case of crash stop the watchdog thread
    exception_processCrashed = true;

    bool ok = exception_createMiniDump(pExceptionInfo);


    // Get module name based on the exception address
    char moduleName[MAX_PATH] = {0};
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(pExceptionInfo->ExceptionRecord->ExceptionAddress, &mbi, sizeof(mbi)) != 0) {
        // The AllocationBase is typically the module's base address.
        GetModuleFileNameA((HMODULE)mbi.AllocationBase, moduleName, sizeof(moduleName));
    }
    // Get the filename
    char* p = strrchr(moduleName, '\\');
    if (p != NULL) {
        strcpy(moduleName, p + 1);
    }

    // Send diagnostics data to server
    char* udpPayload = va(
        //crashData "CoD2x MP"      "1.0.0"          "win-x86" "{HWID}" "{CODE}" "{ADDRESS}" "{MODULE_NAME}"
        "crashData \"CoD2x MP\" \"" APP_VERSION "\" \"win-x86\" \"%s\" \"0x%08x\" \"0x%p\" \"%s\"\n", 
        "0", (unsigned int)(pExceptionInfo->ExceptionRecord->ExceptionCode), pExceptionInfo->ExceptionRecord->ExceptionAddress, moduleName);
    NET_OutOfBandPrint(NS_CLIENT, updater_address, udpPayload);


    DWORD exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;

    // Show message box
    char tempBuffer[1024];
    sprintf(tempBuffer, 
        "Call of Duty 2 has crashed.\n"
        "\n"
        "Exception Code:\t\t"   "0x%08x  (%s)\n"
        "Exception Address:\t"  "0x%p  (%s)\n"
        "\n"
        "%s",
        (unsigned int)(exceptionCode), exception_getText(exceptionCode),
        pExceptionInfo->ExceptionRecord->ExceptionAddress, (moduleName[0] ? moduleName : "unknown module"),
        ok ? "Diagnostic file 'CoD2MP_s.crash.dmp' saved into CoD2 folder.\nPlease send this file to the developers." : "");

    MessageBox(NULL, tempBuffer, "Application crash", MB_OK | MB_ICONERROR | MB_TOPMOST);


    // Return EXCEPTION_EXECUTE_HANDLER to let the app exit normally (or EXCEPTION_CONTINUE_SEARCH)
    return EXCEPTION_EXECUTE_HANDLER;
}


/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void exception_init() {
    // Set the unhandled exception filter so that our CrashHandler is called if the app crashes.
    SetUnhandledExceptionFilter(exception_handler);
}
