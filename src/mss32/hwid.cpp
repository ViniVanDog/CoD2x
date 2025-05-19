/*
 * hwid.cpp - Hardware ID (HWID) Management System
 *
 * This module provides functions to generate and manage HWIDs based on system components such as CPU ID and Hard Drive Serial.
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

dvar_t *cl_hwid;

/**
 * Retrieves a system hardware or configuration property via WMI (Windows Management Instrumentation).
 *
 * WMI is a Microsoft technology that provides a unified interface for querying system information,
 * including hardware details (like BIOS serial, CPU ID, motherboard UUID), operating system config,
 * and many other management data sources. It is built on top of COM (Component Object Model),
 * and exposes information through a set of namespaces, classes, and properties.
 *
 * This function:
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
HRESULT hwid_getWMIProperty(const wchar_t *wmiClass, const wchar_t *property, char *output, size_t maxLen)
{
    HRESULT hr;
    IWbemLocator *pLocator = NULL;
    IWbemServices *pServices = NULL;
    IEnumWbemClassObject *pEnumerator = NULL;
    IWbemClassObject *pObj = NULL;
    ULONG uReturned = 0;
    VARIANT vtProp;

    *output = '\0';

    hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (void **)&pLocator);
    if (FAILED(hr))
        return hr;

    hr = pLocator->ConnectServer(SysAllocString(L"ROOT\\CIMV2"), NULL, NULL, 0, 0, 0, NULL, &pServices);
    if (FAILED(hr))
        goto cleanup;

    // Set security for the WMI connection (authentication & impersonation settings)
    hr = CoSetProxyBlanket(pServices,
                           RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL, EOAC_NONE);
    if (FAILED(hr))
        goto cleanup;

    wchar_t query[256];
    swprintf(query, sizeof(query) / sizeof(wchar_t), L"SELECT * FROM %ls", wmiClass);

    hr = pServices->ExecQuery(
        SysAllocString(L"WQL"),
        SysAllocString(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (FAILED(hr))
        goto cleanup;

    // Get the first result from the query
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturned);
    if (FAILED(hr) || uReturned == 0)
    {
        hr = WBEM_E_NOT_FOUND;
        goto cleanup;
    }

    // Retrieve the specified property value from the object
    VariantInit(&vtProp);
    hr = pObj->Get(property, 0, &vtProp, 0, 0);
    if (SUCCEEDED(hr) && vtProp.vt == VT_BSTR && vtProp.bstrVal != NULL)
    {
        WideCharToMultiByte(CP_ACP, 0, vtProp.bstrVal, -1, output, (int)maxLen, NULL, NULL);
    }
    VariantClear(&vtProp);

cleanup:
    if (pObj)
        pObj->Release();
    if (pEnumerator)
        pEnumerator->Release();
    if (pServices)
        pServices->Release();
    if (pLocator)
        pLocator->Release();
    return hr;
}

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

/**
 * Generates a unique Hardware ID (HWID) based on the system's hard drive serial number and CPU ID.
 *
 * @return A pointer to the generated HWID string.
 */
int hwid_generate()
{
    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        SHOW_ERROR("Failed to generate HWID. COM init failed: 0x%08X\n", hr);
        return 0;
    }

    struct WMIProperty {
        const wchar_t *wmiClass;
        const wchar_t *property;
        char *buffer;
        size_t bufferSize;
    };
    
    char bios[128] = "", cpu[128] = "", board[128] = "", uuid[128] = "";

    WMIProperty wmiProperties[] = {
        {L"Win32_BIOS", L"SerialNumber", bios, sizeof(bios)},
        {L"Win32_Processor", L"ProcessorId", cpu, sizeof(cpu)},
        {L"Win32_BaseBoard", L"SerialNumber", board, sizeof(board)},
        {L"Win32_ComputerSystemProduct", L"UUID", uuid, sizeof(uuid)}
    };

    for (int i = 0; i < 4; i++) {
        HRESULT result = hwid_getWMIProperty(wmiProperties[i].wmiClass, wmiProperties[i].property, wmiProperties[i].buffer, wmiProperties[i].bufferSize);
        if (FAILED(result)) {
            SHOW_ERROR("Failed to generate HWID. Failed to get WMI property %ls.%ls: 0x%08X", 
                       wmiProperties[i].wmiClass, wmiProperties[i].property, result);
            return 0;
        }
    }

    CoUninitialize();

    char combined[512];
    snprintf(combined, sizeof(combined), "%s %s %s %s", bios, cpu, board, uuid);

    // Verify HWID does not change for eyza's PC
    #if DEBUG && defined(DEVELOPER)
        if (strcmp(DEVELOPER, "TOMAS-PC_tomas") == 0) {
            if (strcmp(combined, "System Serial Number BFEBFBFF000B0671 221213970402628 8F71A255-391A-3529-B885-581122D0358F") != 0)
            {
                SHOW_ERROR("HWID changed: %s", combined);
            }
        }
    #endif

    //MessageBoxA(NULL, combined, "HWID", MB_OK);

    uint32_t hash;
    hwid_hash(combined, &hash);

    return hash;
}

/** Called every frame at the start of the frame. */
void hwid_frame()
{
}

/** Called once at game start after common initialization. Used to initialize variables, cvars, etc. */
void hwid_init()
{
    cl_hwid = Dvar_RegisterInt("cl_hwid", hwid_generate(), INT32_MIN, INT32_MAX, (dvarFlags_e)(DVAR_USERINFO | DVAR_NOWRITE));
}

/** Called before the entry point is executed. Used to patch memory. */
void hwid_patch()
{
}