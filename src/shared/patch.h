#ifndef MEMORY_PATCH_H
#define MEMORY_PATCH_H

#include <stdint.h>

void patch_copy(unsigned int targetAddress, void* source, unsigned int length);
void patch_byte(unsigned int targetAddress, uint8_t value);
void patch_int32(unsigned int targetAddress, int32_t value);
void patch_float(unsigned int targetAddress, float value);
void patch_string_ptr(unsigned int targetAddress, const char * value);
void* patch_call(unsigned int targetAddress, unsigned int destinationAddress);
void* patch_jump(unsigned int targetAddress, unsigned int destinationAddress);
void patch_nop(unsigned int startAddress, unsigned int length, void* originalBytes = NULL);

#endif
