#include "shared.h"

#if COD2X_WIN32
#include <windows.h>
#endif

#if COD2X_LINUX
#include <memory.h>
#endif

/**
 * Enum representing the type of patch action to perform.
 */
typedef enum {
    PATCH_ACTION_COPY,
    PATCH_ACTION_BYTE,
    PATCH_ACTION_INT32,
    PATCH_ACTION_STRING_POINTER,
    PATCH_ACTION_CALL,
    PATCH_ACTION_JUMP,
    PATCH_ACTION_PUSH,
    PATCH_ACTION_NOP
} PatchAction;

/**
 * Centralized function to handle memory protection, instruction cache flushing, 
 * and memory modification based on the specified action.
 * 
 * @param targetAddress The memory address to patch.
 * @param source A pointer to the source data (varies by action type).
 * @param length The number of bytes to modify.
 * @param action The type of patch action to perform.
 */
void patch_memory(unsigned int targetAddress, const void* source, unsigned int length, PatchAction action, void* returnValue = NULL) {

    #if COD2X_WIN32
    DWORD oldProtection;
    // Change the memory protection to allow writing
    VirtualProtect((void*)targetAddress, length, PAGE_EXECUTE_READWRITE, &oldProtection);
    #endif

    switch (action) {
        case PATCH_ACTION_COPY:
            memcpy((void*)targetAddress, source, length);
            break;
        case PATCH_ACTION_BYTE:
            *(uint8_t*)targetAddress = *(uint8_t*)source;
            break;
        case PATCH_ACTION_INT32:
            *(int32_t*)targetAddress = *(int32_t*)source;
            break;
        case PATCH_ACTION_STRING_POINTER:
            *(const char **)(targetAddress) = (const char*)source;
            break;
        case PATCH_ACTION_CALL:
        case PATCH_ACTION_JUMP: {
            // If a return value pointer is provided, store the original target address.
            if (returnValue != NULL) {
                int originalRelativeOffset;
                memcpy(&originalRelativeOffset, (void*)(targetAddress + 1), 4);
                unsigned int originalTarget = (unsigned int)(targetAddress + 5 + originalRelativeOffset);
                *(unsigned int*)returnValue = originalTarget;
            }
            unsigned char opcode = (action == PATCH_ACTION_CALL) ? 0xE8 : 0xE9;
            *(unsigned char*)targetAddress = opcode;
            int relativeOffset = *(unsigned int*)source - (targetAddress + 5);
            memcpy((void*)(targetAddress + 1), &relativeOffset, 4);
            break;
        }
        case PATCH_ACTION_PUSH:
            *(unsigned char*)targetAddress = 0x68;
            memcpy((void*)(targetAddress + 1), &source, sizeof(source));
            break;
        case PATCH_ACTION_NOP:
            if (returnValue != NULL) {
                // Store the original bytes before patching with NOPs
                memcpy(returnValue, (void*)targetAddress, length);
            }
            memset((void*)targetAddress, 0x90, length);
            break;
    }

    #if COD2X_WIN32
        // Flush the CPU instruction cache to ensure the modified instructions are used
        FlushInstructionCache(GetCurrentProcess(), (void*)targetAddress, length);

        // Restore the original memory protection
        VirtualProtect((void*)targetAddress, length, oldProtection, &oldProtection);
    #endif
}


/**
 * Modifies the memory at a given memory region with data from a given source, similar to memcpy.
 * 
 * Example:
 *  patch_copy(0x400000, "abc", 3);
 *  0x400000:  01 01 01 01 01         // before
 *  0x400000:  61 62 63 01 01         // after
 */
void patch_copy(unsigned int targetAddress, void* source, unsigned int length) {
    patch_memory(targetAddress, source, length, PATCH_ACTION_COPY);
}

/**
 * Modifies the memory at a given address to insert a single byte value.
 * 
 * Example:
 *  patch_byte(0x400000, 0x90);
 *  0x400000:  01 01 01 01 01         // before
 *  0x400000:  90 01 01 01 01         // after
 */
void patch_byte(unsigned int targetAddress, uint8_t value) {
    patch_memory(targetAddress, &value, 1, PATCH_ACTION_BYTE);
}

/**
 * Modifies the memory at a given address to insert a 32-bit integer value.
 * 
 * Example:
 *  patch_int32(0x400000, 1234);
 *  0x400000:  01 01 01 01         // before
 *  0x400000:  D2 04 00 00         // after
 */
void patch_int32(unsigned int targetAddress, int32_t value) {
    patch_memory(targetAddress, &value, sizeof(int32_t), PATCH_ACTION_INT32);
}

/**
 * Modifies the memory at a given address to insert an address of string pointer.
 * 
 * Example:
 *  patch_string_ptr(0x400000, "hi");
 *  0x400000:  01 01 01 01         // before
 *  0x400000:  xx xx xx xx         // after (address of "hi")
 */
void patch_string_ptr(unsigned int targetAddress, const char * value) {
    patch_memory(targetAddress, value, sizeof(const char *), PATCH_ACTION_STRING_POINTER);
}

/**
 * Modifies the memory at a given address to insert a relative call to a new location.
 * It will replace 5 bytes.
 * Returns the original target address before the patch.
 * 
 * Example:
 *  patch_call(0x400000, 0x500000);
 *  0x400000:  e8 88 87 ee ff  call    sub_466460       // before
 *  0x400000:  e8 xx xx xx xx  call    newFunction      // after
 */
void* patch_call(unsigned int targetAddress, unsigned int destinationAddress) {
    void* returnValue = NULL;
    patch_memory(targetAddress, &destinationAddress, 5, PATCH_ACTION_CALL, &returnValue);
    return returnValue;
}

/**
 * Modifies the memory at a given address to insert an unconditional jump (JMP) to a new location.
 * It will replace 5 bytes.
 * Returns the original target address before the patch.
 * 
 * Example:
 *  patch_jump(0x400000, 0x500000);
 *  0x400000:  e9 f8 7f ee ff  jmp    sub_466460       // before
 *  0x400000:  e9 xx xx xx xx  jmp    newFunction      // after
 */
void* patch_jump(unsigned int targetAddress, unsigned int destinationAddress) {
    void* returnValue = NULL;
    patch_memory(targetAddress, &destinationAddress, 5, PATCH_ACTION_JUMP, &returnValue);
    return returnValue;
}

/**
 * Modifies the memory at a given address with NOP (No Operation) instructions.
 * 
 * This function replaces `length` bytes starting from `startAddress` with NOP instructions.
 * 
 * Example:
 *  patch_nop(0x400000, 2);
 *  0x400000:  00 00 00 00 00         // before
 *  0x400000:  90 90 00 00 00         // after
 */
void patch_nop(unsigned int startAddress, unsigned int length, void* originalBytes) {
    patch_memory(startAddress, NULL, length, PATCH_ACTION_NOP, originalBytes);
}
