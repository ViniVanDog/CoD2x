#ifndef SHARED_ALL_H
#define SHARED_ALL_H

#include <stdint.h> // uint8_t, uint32_t, etc.
#include <stdarg.h> // va_list, va_start, va_end
#include <stdlib.h> // atoi, etc.
#include <string.h> // strncpy, etc.

// CoD2x is currecntly supported on Windows and Linux
// Based on the platform, the appropriate code will be compiled
#if _WIN32 == 1
    #define COD2X_WIN32 1
    #define COD2X_LINUX 0
#else
    #define COD2X_WIN32 0
    #define COD2X_LINUX 1
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)



// Application-wide constants
#define APP_NAME "CoD2x"
#define APP_URL "http://cod2x.me/"


// Original servers
#define SERVER_ACTIVISION_AUTHORIZE_URI "cod2master.activision.com"
#define SERVER_ACTIVISION_AUTHORIZE_PORT 20700
#define SERVER_ACTIVISION_MASTER_URI "cod2master.activision.com"
#define SERVER_ACTIVISION_MASTER_PORT 20710

// CoD2x servers
#if 1
    #define SERVER_MASTER_URI "master.cod2x.me"
    #define SERVER_MASTER_PORT 20710
    #define SERVER_UPDATE_URI "master.cod2x.me"
    #define SERVER_UPDATE_PORT 20720
#else
    #define SERVER_MASTER_URI "127.0.0.1"
    #define SERVER_MASTER_PORT 20710
    #define SERVER_UPDATE_URI "127.0.0.1"
    #define SERVER_UPDATE_PORT 20720
#endif



// Game protocol version,  115=1.0, 116=1.1, 117=1.3, 118=1.3, 119=1.3.Miscrosoft
// CoD2x servers will have the new protocol number
// When joining a server, the client will use the original 118 protocol to be able to connect old servers
#define PROTOCOL_VERSION 120    // original 118




// Platform-specific macro to get the address
#if COD2X_WIN32
    #define ADDR(addr_win, addr_linux) (addr_win)
    // Choose between Windows (W) or Linux (L) code
    #define WL(win, linux) win
#endif
#if COD2X_LINUX
    #define ADDR(addr_win, addr_linux) (addr_linux)
    // Choose between Windows (W) or Linux (L) code
    #define WL(win, linux) linux
    #define __cdecl
#endif

#if DEBUG
    #define DEBUG_RELEASE(debug, release) debug
#else
    #define DEBUG_RELEASE(debug, release) release
#endif


#include "version.h"
#include "patch.h"
#include "assembly.h"
#include "cod2_common.h"
#include "cod2_shared.h"
#include "cod2_math.h"
#include "cod2_dvars.h"
#include "cod2_entity.h"
#include "cod2_player.h"
#include "cod2_cmd.h"
#include "cod2_file.h"
#include "cod2_net.h"
#include "cod2_server.h"
#if COD2X_WIN32
#include "cod2_client.h"
#endif

void escape_string(char* buffer, size_t bufferSize, const void* data, size_t length);

#endif

