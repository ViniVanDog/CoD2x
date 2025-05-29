#include "exception.h"

#include <windows.h>
#include <stdio.h>
#include <dbghelp.h>

#include "shared.h"
#include "../shared/cod2_shared.h"
#include "../shared/cod2_net.h"
#include "../shared/cod2_dvars.h"

volatile int exception_processCrashed = false;
extern struct netaddr_s updater_address;
extern dvar_t *cl_hwid;


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

    // If the process already crashed, it means an error happends in this handler (maybe due to NET is not initialized yet)
    if (exception_processCrashed == 2) {
        MessageBoxA(NULL, "Application is already crashed. Cannot handle the exception again.", "Application error", MB_OK | MB_ICONERROR | MB_TOPMOST);
        return EXCEPTION_CONTINUE_SEARCH; // Continue searching for another handler
    }

    bool ignoreErrorDetails = false;
    if (exception_processCrashed == 1) {
        ignoreErrorDetails = true; // Ignore error details if the process crashed again
    }

    // In case of crash stop the watchdog thread
    exception_processCrashed++;


    char moduleName[MAX_PATH] = {0};
    unsigned int moduleBase = 0;
    unsigned int fileOffset = 0;
    const size_t stackDumpSize = 64; // number of DWORDs to dump
    const size_t perLineSize = 128; // generous estimate per line
    char stackDump[stackDumpSize * perLineSize]; // increased buffer size
    strcpy(stackDump, "Error reading stack");

    unsigned int exceptionCode = pExceptionInfo->ExceptionRecord->ExceptionCode;
    unsigned int exceptionAddress = (unsigned int)pExceptionInfo->ExceptionRecord->ExceptionAddress;

    if (!ignoreErrorDetails) 
    {
        // Get module name based on the exception address
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(pExceptionInfo->ExceptionRecord->ExceptionAddress, &mbi, sizeof(mbi)) != 0) {
            // Get the filename
            GetModuleFileNameA((HMODULE)mbi.AllocationBase, moduleName, sizeof(moduleName));
            char* p = strrchr(moduleName, '\\');
            if (p != NULL) {
                strcpy(moduleName, p + 1);
            }
            // Calculate the real file offset of the exception address        
            moduleBase = (unsigned int)mbi.AllocationBase;
            fileOffset = exceptionAddress - moduleBase;
        }

        // Get the value of the ESP register at the time of exception
        uintptr_t espValue = 0;
        if (pExceptionInfo && pExceptionInfo->ContextRecord) {
            espValue = pExceptionInfo->ContextRecord->Esp;
        }

        size_t written = 0;
        stackDump[0] = '\0'; // Initialize stackDump to an empty string

        // First line will contain the exception details
        written += snprintf(stackDump + written, sizeof(stackDump) - written, "0x%08X 0x%08X %s\n", moduleBase, fileOffset, moduleName);

        // Read a portion of the stack
        if (espValue) {
            DWORD* stack = (DWORD*)espValue;

            for (size_t i = 0; i < stackDumpSize && written < sizeof(stackDump) - perLineSize; ++i) {
                
                // Check if the stack pointer is valid, filtering out invalid pointers
                if (IsBadReadPtr(&stack[i], sizeof(DWORD)))
                    break;

                unsigned int stackValue = (unsigned int)(stack[i]);

                // Check if the stack value is a pointer to valid memory, filtering out stack values
                if (IsBadReadPtr((void*)stackValue, sizeof(DWORD) * stackDumpSize)) {
                    continue;
                }
                            
                MEMORY_BASIC_INFORMATION mbi2;
                if (VirtualQuery((void*)stackValue, &mbi2, sizeof(mbi2)) == 0)
                    continue;
                        
                // Check if the stack pointer is within an executable memory region, filtering out non-executable regions like data segments
                if (!(mbi2.Protect & PAGE_EXECUTE) &&
                    !(mbi2.Protect & PAGE_EXECUTE_READ) &&
                    !(mbi2.Protect & PAGE_EXECUTE_READWRITE) &&
                    !(mbi2.Protect & PAGE_EXECUTE_WRITECOPY))
                    continue;

                // Get the module name
                char moduleName2[MAX_PATH] = {0};
                GetModuleFileNameA((HMODULE)mbi2.AllocationBase, moduleName2, sizeof(moduleName2));
                char* p2 = strrchr(moduleName2, '\\');
                if (p2 != NULL) {
                    strcpy(moduleName2, p2 + 1);
                }
                if (moduleName2[0] == '\0')
                    continue;

                // If the exception address is 0, use first valid address from the stack
                // Exception address is 0 if there is a call to null pointer
                if (exceptionAddress == 0 && strncmp(moduleName2, moduleName, MAX_PATH) == 0) {
                    exceptionAddress = stackValue;
                    fileOffset = stackValue - (unsigned int)mbi2.AllocationBase;
                }
                
                written += snprintf(stackDump + written, sizeof(stackDump) - written, "0x%08X 0x%08X %s\n", stackValue, stackValue - (unsigned int)mbi2.AllocationBase, moduleName2);
            }
        } else {
            strcpy(stackDump, "Stack unavailable");
        }
    }

    if (updater_address.type > NA_BAD) { // avoid another crash if updater is not initialized yet
        int hwid = cl_hwid ? cl_hwid->value.integer : 0;
        // Send diagnostics data to server
        char* udpPayload = va(
            //crashData "CoD2x MP"      "1.0.0"          "win-x86" "{HWID}" "{CODE}" "{ADDRESS}" "{MODULE_NAME}" "{MODULE_OFFSET}" "{STACK_DUMP}"
            "crashData \"CoD2x MP\" \"" APP_VERSION "\" \"win-x86\" \"%i\" \"0x%08x\" \"0x%p\" \"%s\" \"0x%p\" \"%s\"\n", 
            hwid, (unsigned int)(exceptionCode), exceptionAddress, moduleName, fileOffset, stackDump);
        NET_OutOfBandPrint(NS_CLIENT, updater_address, udpPayload);
    }

    // Ask user if they want to create a debug file
    char tempBuffer[1024];
    sprintf(tempBuffer, 
        "Call of Duty 2 has crashed.\n"
        "\n"
        "Code:\t\t"   "0x%08x  (%s)\n"
        "Address:\t\t"  "0x%08x  (%s)\n"
        "File Address:\t"  "0x%08x\n"
        "MSS32 version:\t"  "%s\n"
        "\n"
        "Stack:\n"  "%s\n"
        "\n"
        "Would you like to create a crash dump file before exiting?\n"
        "This file can help developers diagnose the problem.",
        exceptionCode, exception_getText(exceptionCode),
        exceptionAddress, (moduleName[0] ? moduleName : "unknown module"),
        fileOffset,
        APP_VERSION,
        stackDump);

    int result = MessageBox(NULL, tempBuffer, "Application crash", MB_YESNO | MB_ICONERROR | MB_TOPMOST);

    if (result == IDYES) {
        bool dumpOk = exception_createMiniDump(pExceptionInfo);
        if (dumpOk) {
            MessageBox(NULL, 
                "Diagnostic file 'CoD2MP_s.crash.dmp' saved into CoD2 folder.\n"
                "\n"
                "Please send this file to the developers.\n"
                "You can reach them on our discord, more info at https://cod2x.me/.\n"
                "\n"
                "Without this file we would not have a chance to fix this problem. Thank you for your help!"
            , "Crash Dump Created", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        } else {
            MessageBox(NULL, "Failed to create crash dump file.", "Crash Dump Error", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        }
    }


    // Return EXCEPTION_EXECUTE_HANDLER to let the app exit normally (or EXCEPTION_CONTINUE_SEARCH)
    return EXCEPTION_EXECUTE_HANDLER;
}


/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void exception_init() {
    // Set the unhandled exception filter so that our CrashHandler is called if the app crashes.
    SetUnhandledExceptionFilter(exception_handler);
}
