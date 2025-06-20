/*
 * hwid.cpp - Hardware ID (HWID) Management System
 *
 * This module provides functions to generate and manage HWIDs based on system components.
 * It now uses MD5 hashing to generate a unique identifier.
 */
#include "hwid.h"

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <iphlpapi.h>
#include <netioapi.h>
#include <assert.h>
#include <cpuid.h>

#include <initguid.h>
#include <wbemidl.h>

#include "shared.h"
#include "../shared/cod2_dvars.h"
#include "../shared/cod2_cmd.h"
#include "../shared/cod2_client.h"

dvar_t *cl_hwid;
dvar_t *cl_hwid2;

// Global flags and buffers
bool hwid_changed = false;               // Indicates if HWID changed since last check
char hwid_old[33] = {};                  // Stores the previous HWID
char hwid_regid[33] = {};                // Stores the registry ID

// Global buffers to cache all WMI properties (shared by both generate functions)
char hwid_bios_manufacturer[128] = {};
char hwid_bios_serial[128] = {};
char hwid_cpu_name[128] = {};
char hwid_cpu_id[128] = {};
char hwid_board_manufacturer[128] = {};
char hwid_board_serial[128] = {};
char hwid_uuid[128] = {};
char hwid_diskModel[128] = {};
char hwid_totalMem[128] = {};
char hwid_gpuName[128] = {};

struct WMIProperty {
    const wchar_t *wmiClass;
    const wchar_t *property;
    char *buffer;
    size_t bufferSize;
};


/**
 * Generates an hash from the given input string.
 *
 * @param str The input string to hash.
 * @param output The output buffer to store the hash result
 */
void hwid_hash(const char *str, uint32_t *output)
{
    // FVN-1a hash
    uint32_t hash = 2166136261u;
    while (*str)
    {
        hash ^= (unsigned char)(*str++);
        hash *= 16777619;
    }

    hash &= 0x7FFFFFFF; // clear highest bit (bit 31) to avoid negative numbers
    if (hash == 0)
        hash = 1; // avoid returning zero

    *output = hash;
}



/*
 * WMI is a Microsoft technology that provides a unified interface for querying system information,
 * including hardware details (like BIOS serial, CPU ID, motherboard UUID), operating system config,
 * and many other management data sources. It is built on top of COM (Component Object Model),
 * and exposes information through a set of namespaces, classes, and properties.
 *
 * Steps:
 *   - Initializes COM and connects to the default WMI namespace: "ROOT\\CIMV2"
 *   - Constructs and executes a WQL (WMI Query Language) query
 *   - Retrieves the first result (WMI object) returned by the query
 *   - Extracts the value of the specified property from that object
 *   - Converts the value (if it's a BSTR string) into a regular multibyte C string (char*) for use
 *
 * Note:
 *   - This function assumes the property is a VT_BSTR string (which is true for most hardware IDs)
 *   - This function uses COM objects and should be called after CoInitializeEx()
 *   - Caller is responsible for calling CoUninitialize() after all COM usage is complete
 */

/**
 * Helper to read a string property from a WMI query result
 */
static HRESULT hwid_readWMIString(IWbemServices* pServices, const wchar_t* wmiClass, const wchar_t* prop, char* out, size_t outSize)
{
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemClassObject* pObj = NULL;
    VARIANT vtProp;
    ULONG uReturned = 0;
    out[0] = '\0';

    wchar_t query[256];
    swprintf(query, sizeof(query)/sizeof(wchar_t), L"SELECT * FROM %ls", wmiClass);
    HRESULT hr = pServices->ExecQuery(SysAllocString(L"WQL"),
                                      SysAllocString(query),
                                      WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                      NULL, &pEnumerator);
    if (FAILED(hr) || !pEnumerator)
        return hr;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturned);
    if (FAILED(hr) || uReturned == 0 || !pObj) {
        if (pEnumerator) pEnumerator->Release();
        return hr;
    }
    VariantInit(&vtProp);
    hr = pObj->Get(prop, 0, &vtProp, 0, 0);
    if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != NULL)
        WideCharToMultiByte(CP_ACP, 0, vtProp.bstrVal, -1, out, (int)outSize, NULL, NULL);
    else
        out[0] = '\0';
    VariantClear(&vtProp);
    pObj->Release();
    pEnumerator->Release();
    return hr;
}

/**
 * Loads all required WMI properties into shared global variables.
 */
HRESULT hwid_loadProperties(char* errorBuffer, size_t errorBufferSize)
{
    HRESULT hr;
    IWbemLocator *pLocator = NULL;
    IWbemServices *pServices = NULL;

    // Read all required WMI properties
    // Define all required WMI properties in an array
    WMIProperty properties[] = {
        {L"Win32_BIOS", L"Manufacturer", hwid_bios_manufacturer, sizeof(hwid_bios_manufacturer)},
        {L"Win32_BIOS", L"SerialNumber", hwid_bios_serial, sizeof(hwid_bios_serial)},
        {L"Win32_Processor", L"Name", hwid_cpu_name, sizeof(hwid_cpu_name)},
        {L"Win32_Processor", L"ProcessorId", hwid_cpu_id, sizeof(hwid_cpu_id)},
        {L"Win32_BaseBoard", L"Manufacturer", hwid_board_manufacturer, sizeof(hwid_board_manufacturer)},
        {L"Win32_BaseBoard", L"SerialNumber", hwid_board_serial, sizeof(hwid_board_serial)},
        {L"Win32_ComputerSystemProduct", L"UUID", hwid_uuid, sizeof(hwid_uuid)},
        {L"Win32_DiskDrive", L"Model", hwid_diskModel, sizeof(hwid_diskModel)},
        {L"Win32_ComputerSystem", L"TotalPhysicalMemory", hwid_totalMem, sizeof(hwid_totalMem)},
        {L"Win32_VideoController", L"Name", hwid_gpuName, sizeof(hwid_gpuName)}
    };

    hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
        return hr;

    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (void **)&pLocator);
    if (FAILED(hr))
        goto cleanup;

    hr = pLocator->ConnectServer(SysAllocString(L"ROOT\\CIMV2"), NULL, NULL, 0, 0, 0, NULL, &pServices);
    if (FAILED(hr))
        goto cleanup;

    hr = CoSetProxyBlanket(pServices,
                           RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL, EOAC_NONE);
    if (FAILED(hr))
        goto cleanup;

    // Iterate and read each property, check for errors
    for (size_t i = 0; i < sizeof(properties)/sizeof(properties[0]); ++i) {
        HRESULT propHr = hwid_readWMIString(pServices, properties[i].wmiClass, properties[i].property, properties[i].buffer, properties[i].bufferSize);
        if (FAILED(propHr)) {
            hr = propHr;
            snprintf(errorBuffer, errorBufferSize, "Failed to read WMI property '%ls.%ls'", properties[i].wmiClass, properties[i].property);
            goto cleanup;
        }
    }

    hr = S_OK; // Success

cleanup:
    if (pServices)
        pServices->Release();
    if (pLocator)
        pLocator->Release();
    CoUninitialize();
    return hr;
}

/**
 * Generates a HWID hash (integer) using a subset of the loaded properties.
 * Combines BIOS serial, CPU ID, Board serial, and UUID.
 */
int hwid_generate()
{
    char combined[512];
    // For this simpler HWID, use BIOS serial, CPU ID, Board serial, and UUID.
    snprintf(combined, sizeof(combined), "%s %s %s %s", hwid_bios_serial, hwid_cpu_id, hwid_board_serial, hwid_uuid);

    //MessageBoxA(NULL, combined, "HWID", MB_OK);

    uint32_t hash;
    hwid_hash(combined, &hash);

    return hash;
}

/**
 * Generates a unique Hardware ID (HWID) based on the system's hardware components
 *
 * @return A pointer to the generated HWID string.
 */
char* hwid_generate2()
{
    char combined[1024];
    snprintf(combined, sizeof(combined),
             "BIOS manufacturer: %s\nBIOS serial: %s\nCPU name: %s\nCPU ID: %s\nMotherboard manufacturer: %s\nMotherboard serial: %s\nUUID: %s\nDisk Model: %s\nTotal Memory: %s\nGPU Name: %s",
             hwid_bios_manufacturer, hwid_bios_serial, hwid_cpu_name, hwid_cpu_id,
             hwid_board_manufacturer, hwid_board_serial, hwid_uuid, hwid_diskModel,
             hwid_totalMem, hwid_gpuName);

    //MessageBoxA(NULL, combined, "HWID", MB_OK);
    /*
    ---------------------------
    HWID
    ---------------------------
    BIOS manufacturer: American Megatrends Inc.
    BIOS serial: System Serial Number
    CPU name: 13th Gen Intel(R) Core(TM) i7-13700K
    CPU ID: BFEBFBFF000B0671
    Motherboard manufacturer: ASUSTeK COMPUTER INC.
    Motherboard serial: 221213970402628
    UUID: 8F71A255-391A-3529-B885-581122D0358F
    Disk Model: Samsung SSD 990 EVO 2TB
    Total Memory: 68448202752
    GPU Name: NVIDIA GeForce GTX 1080 Ti
    ---------------------------
    */

    char* hash = CL_BuildMD5(combined, strlen(combined));

    // Registry update
    HKEY hKey;
    char regHash[64] = {0};
    DWORD dwType = REG_SZ;
    DWORD dwSize = sizeof(regHash);
    hwid_changed = false;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Call of Duty 2", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS)
    {
        LONG regResult = RegQueryValueExA(hKey, "HWID", NULL, &dwType, (LPBYTE)regHash, &dwSize);
        if (regResult == ERROR_SUCCESS && dwType == REG_SZ)
        {
            if (strcmp(regHash, hash) != 0)
            {
                hwid_changed = true;
                strncpy(hwid_old, regHash, sizeof(hwid_old) - 1);
                hwid_old[sizeof(hwid_old) - 1] = '\0';

                // Save previous HWID to HWID_OLD before updating
                RegSetValueExA(hKey, "HWID_OLD", 0, REG_SZ, (const BYTE*)regHash, (DWORD)strlen(regHash) + 1);

                // Value exists and is different, update and set change flag
                RegSetValueExA(hKey, "HWID", 0, REG_SZ, (const BYTE*)hash, (DWORD)strlen(hash) + 1);
            }
            strcpy(hash, regHash); // Use registry value
        }
        else
        {
            // Value does not exist, save it
            RegSetValueExA(hKey, "HWID", 0, REG_SZ, (const BYTE*)hash, (DWORD)strlen(hash) + 1);
        }
        RegCloseKey(hKey);
    }
    else
    {
        SHOW_ERROR("Failed to open registry key for HWID.");
        exit(1);
    }

    return hash;
}

/**
 * Generates a unique REGID for the game, used for computer identification.
 * This REGID is used to verify if the HWID is unique accross multiple computers.
 */
void hwid_generate_regid() {
    HKEY hKey;
    char regid[64] = {0};
    DWORD dwType = REG_SZ;
    DWORD dwSize = sizeof(regid);

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Activision\\Call of Duty 2", 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS)
    {
        LONG regResult = RegQueryValueExA(hKey, "REGID", NULL, &dwType, (LPBYTE)regid, &dwSize);
        if (regResult != ERROR_SUCCESS || dwType != REG_SZ || regid[0] == '\0')
        {
            // Generate new random string and hash it
            char randomStr[64];
            snprintf(randomStr, sizeof(randomStr), "%u_%u", (unsigned int)GetTickCount(), rand());
            char* regidHash = CL_BuildMD5(randomStr, strlen(randomStr));
            RegSetValueExA(hKey, "REGID", 0, REG_SZ, (const BYTE*)regidHash, (DWORD)strlen(regidHash) + 1);

            strncpy(hwid_regid, regidHash, sizeof(hwid_regid) - 1);
            hwid_regid[sizeof(hwid_regid) - 1] = '\0';
        } else
        {
            // Use existing REGID
            strncpy(hwid_regid, regid, sizeof(hwid_regid) - 1);
            hwid_regid[sizeof(hwid_regid) - 1] = '\0';
        }
        RegCloseKey(hKey);
    }
    else
    {
        SHOW_ERROR("Failed to open registry key for HWID REGID.");
        exit(1);
    }
}


/** Called every frame at the start of the frame. */
void hwid_frame()
{
    // Check if HWID has changed since last check
    if (hwid_changed) {
        hwid_changed = false;
        Com_Error(ERR_DROP, 
            "Your hardware ID has changed.\n\n"
            "If you were banned, this new hardware ID will also be banned. "
            "If not, you may continue playing. "
            "If this happens often, please contact developers of CoD2x.");
    }
}

/** Called once at game start after common initialization. Used to initialize variables, cvars, etc. */
void hwid_init()
{

    HRESULT hr;
    char errorBuffer[256];
    while ((hr = hwid_loadProperties(errorBuffer, sizeof(errorBuffer))) != S_OK) {
        showErrorMessage("Fatal Error", "Error while generating HWID. Unable to load WMI properties.\nError: 0x%08X %s", hr, errorBuffer);
        int result = MessageBoxA(NULL, 
            "Do you want to retry or exit?\n\nIf the problem persists, please restart your computer and try again.", 
            "Retry or Exit", 
            MB_RETRYCANCEL | MB_ICONQUESTION);
        if (result == IDRETRY)
            Sleep(500);
        else {
            exit(1);
            return;
        }
    }
    
    cl_hwid = Dvar_RegisterInt("cl_hwid", hwid_generate(), INT32_MIN, INT32_MAX, (dvarFlags_e)(DVAR_USERINFO | DVAR_NOWRITE));
    cl_hwid2 = Dvar_RegisterString("cl_hwid2", hwid_generate2(), (dvarFlags_e)(DVAR_USERINFO | DVAR_NOWRITE));

    hwid_generate_regid();
}

/** Called before the entry point is executed. Used to patch memory. */
void hwid_patch()
{
}