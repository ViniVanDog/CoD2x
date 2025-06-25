#ifndef ERROR_H
#define ERROR_H

// Send error data to the updater server
void error_sendCrashData(unsigned int exceptionCode, unsigned int exceptionAddress, const char* moduleName, unsigned int fileOffset, const char* stackDump);

// Send error data to the updater server
void error_sendErrorData(const char* message);

void error_init();
void error_patch();

#endif