#ifndef COD2_SHARED_H
#define COD2_SHARED_H

#include "shared.h"
#include <stdio.h>
#if COD2X_LINUX
#include <strings.h>
#endif

#define	MAX_INFO_STRING		1024



#define va ((char* (*)(const char *, ...))ADDR(0x0044a990, 0x080b7fa6))



// Compare two strings ignoring case
inline int Q_stricmp(const char *s1, const char *s2) {
    #if COD2X_WIN32
        return stricmp(s1, s2);
    #endif
    #if COD2X_LINUX
        return strcasecmp(s1, s2);
    #endif
}

inline void Q_strncpyz( char *dest, const char *src, int destsize )
{
	strncpy( dest, src, destsize-1 );
	dest[destsize-1] = 0;
}


inline void Info_SetValueForKey(const char* buffer, const char* keyName, const char* value) {  
    #if COD2X_WIN32
        const void* original_func = (void*)0x0044ae10; // (char* buffer, char* keyName @ eax, int32_t value)
        ASM( push,     value          ); // 3th argument
        ASM( push,     buffer         ); // 2rd argument
        ASM( mov,      "eax", keyName ); // 1nd argument
        ASM( call,     original_func  );
        ASM( add_esp,  8              ); // Clean up the stack (2 arguments × 4 bytes = 8)
    #endif
    #if COD2X_LINUX
        ((char* (*)(const char* buffer, const char* keyName, const char* value))0x080b85ce)(buffer, keyName, value);
    #endif
}


inline char* Info_ValueForKey(const char* buffer, const char* keyName) {   
    #if COD2X_WIN32
        const void* original_func = (void*)(0x0044aa90); // char* Info_ValueForKey(char* buffer @ ecx, char* key)
        char* result;
        ASM( push,     keyName          ); // 2nd argument                    
        ASM( mov,      "ecx", buffer    ); // 1st argument
        ASM( call,     original_func    ); 
        ASM( add_esp,  4                ); // Clean up the stack (1 argument × 4 bytes = 4)     
        ASM( movr,     result, "eax"    ); // Store the return value in the 'result' variable
        return result;
    #endif
    #if COD2X_LINUX
        return ((char* (*)(const char* buffer, const char* keyName))0x080b8108)(buffer, keyName);
    #endif
}

#endif