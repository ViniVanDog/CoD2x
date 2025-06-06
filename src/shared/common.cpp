#include "common.h"

#include "shared.h"
#include "cod2_common.h"

#define MAX_CONSOLE_LINES 32
#define com_consoleLines ((char**)ADDR(0x00c26110, 0x081a21e0))
#define com_numConsoleLines (*(int*)ADDR(0x00c27280, 0x081a21d4))

char common_commandLine[1024] = {0};

void Com_ParseCommandLine( char *commandLine )
{
    bool inq = 0;
    com_numConsoleLines = 0;

    // CoD2x: Copy command line to local buffer
    if (strlen(commandLine) >= sizeof(common_commandLine)) {
        Com_Error(ERR_DROP, "Command line too long: %s", commandLine);
        return;
    }
    strncpy(common_commandLine, commandLine, sizeof(common_commandLine) - 1);
    common_commandLine[sizeof(common_commandLine) - 1] = '\0'; // Ensure null termination
    commandLine = common_commandLine;
    // CoD2x: End

    // CoD2x: Check if whole string is in quotes and remove them
    if (commandLine[0] == '"' && commandLine[strlen(commandLine) - 1] == '"') {
        commandLine++;
        commandLine[strlen(commandLine) - 1] = 0;
    }
    // CoD2x: End

	while ( *commandLine )
	{
        // CoD2x: Allow escaping quotes in string
        if (inq && *commandLine == '\\' && *(commandLine + 1) == '"') {
            commandLine += 2;
            continue;
        }
        // CoD2x: End

		if ( *commandLine == '"' )
		{
			inq = !inq;
		}

        // CoD2x: Loop thru the quote, ignore separators
        else if ( inq )
        {
            commandLine++;
            continue;
        }
        // CoD2x: End

		// look for a + seperating character
		// if commandLine came from a file, we might have real line seperators
		if ( *commandLine == '+' || *commandLine == '\n' || /*CoD2x: comma */ *commandLine == ',' )
		{
			if ( com_numConsoleLines == MAX_CONSOLE_LINES )
			{
				return;
			}

            // CoD2x: dont add empty lines
            if (*(commandLine + 1) != '\0' && *(commandLine + 1) != '+' && *(commandLine + 1) != '\n' && *(commandLine + 1) != ',') { // CoD2x: end
                com_consoleLines[com_numConsoleLines] = commandLine + 1;
                com_numConsoleLines++;
            }
			*commandLine = 0; // terminate previous command
		}
		commandLine++;
	}

    // CoD2x: Debug
    #if DEBUG && 0
        for (int i = 0; i < com_numConsoleLines; i++) {
            Com_Printf("CMD: '%s'\n", com_consoleLines[i]);
        }
    #endif
    // CoD2x: End
}

void Com_ParseCommandLine_Win32() {
    char* commandLine;
    ASM( movr, commandLine, "eax" );
    Com_ParseCommandLine(commandLine);
}



void common_printInfo() {
    Com_Printf("-----------------------------------\n");
    Com_Printf("Command line: ");
    for (int i = 0; i < com_numConsoleLines; i++) {
        if (i > 0) {
            Com_Printf(", ");
        }
        Com_Printf("'%s'", com_consoleLines[i]);
    }
    if (com_numConsoleLines == 0) {
        Com_Printf("''");
    }
    Com_Printf("\n");
    
    Com_Printf("-----------------------------------\n");
    Com_Printf("CoD2x " APP_VERSION " loaded\n");
    Com_Printf("-----------------------------------\n");
}


void common_init() {

}


// Server side hooks
// The hooked functions are the same for both Windows and Linux
void common_patch()
{
    // Print into console when the app is started -> "CoD2 MP 1.3 build win-x86 May  1 2006"
    patch_string_ptr(ADDR(0x00434467 + 1, 0x080620c6 + 4), __DATE__ " " __TIME__);          // originally win: "May  1 2006",  linux: "Jun 23 2006"
    patch_string_ptr(ADDR(0x0043446c + 1, 0x080620ce + 4), ADDR("win-x86", "linux-i386"));  // original
    patch_string_ptr(ADDR(0x00434471 + 1, 0x080620d6 + 4), APP_VERSION);                    // originally "1.3"
    patch_string_ptr(ADDR(0x00434476 + 1, 0x080620de + 4), "CoD2 MP");                      // original
    patch_string_ptr(ADDR(0x0043447b + 1, 0x08062235 + 3), "%s %s build %s %s\n");          // original


    // Value of cvar /version   ->   "CoD2 MP 1.3 build pc_1.3_1_1 Mon May 01 2006 05:05:43PM linux-i386"
    patch_string_ptr(ADDR(0x004346de + 1, 0x08051e1e + 4), __DATE__ " " __TIME__);          // originally "Mon May 01 2006 05:05:43PM"
    patch_string_ptr(ADDR(0x004346e3 + 1, 0x08051e26 + 4), "by eyza");                      // originally "pc_1.3_1_1"
    patch_string_ptr(ADDR(0x004346f7 + 1, 0x08062219 + 4), ADDR("win-x86", "linux-i386"));  // original
    patch_string_ptr(ADDR(0x00434701 + 1, 0x08062225 + 4), APP_VERSION);                    // originally "1.3"
    patch_string_ptr(ADDR(0x00434706 + 1, 0x0806222d + 4), "CoD2 MP");                      // original
    patch_string_ptr(ADDR(0x0043470b + 1, 0x08062235 + 3), "%s %s build %s %s");            // original


    // Hook Com_ParseCommandLine
    patch_call(ADDR(0x004344a8, 0x080620fd), (unsigned int)ADDR(Com_ParseCommandLine_Win32, Com_ParseCommandLine));


    // Fix the port negative number when formatting IP address
    patch_string_ptr(ADDR(0x00447733 + 1, 0x0806b238 + 4), "%i.%i.%i.%i:%hu");          // originally "%i.%i.%i.%i:%i"
    #if COD2X_WIN32
        patch_string_ptr(0x00412143 + 1, "%i.%i.%i.%i:%hu");                         // originally "%i.%i.%i.%i:%i"
    #endif

    // Value of cvar /shortversion   ->   "1.3"
    // Also visible in menu right bottom corner
    patch_string_ptr(ADDR(0x0043477c + 1, 0x08062281 + 4), APP_VERSION);             // originally "1.3"


    // Value of cvar /protocol   ->   "118"
    // Changing this cvar does not involve the protocol number used while connecting to server
    // It only affect server info when asking for /serverinfo or server notify master server
    #if COD2X_WIN32
        patch_byte(0x00459754 + 1, PROTOCOL_VERSION);
    #endif
    #if COD2X_LINUX
        patch_byte(0x08093b2c + 4, PROTOCOL_VERSION);
        patch_byte(0x08093b34 + 4, PROTOCOL_VERSION);
        patch_byte(0x08093b3c + 4, PROTOCOL_VERSION);
    #endif



    // Fix "iwd/sum name mismatch" error (especially for Windows when doing devmap) caused by too many IWD files
    // The problem is in function Info_SetValueForKey_Big, which uses 0x2000 for buffer size, but there is a check for 0x400
    patch_int32(ADDR(0x0044b14b + 1, 0x080b89ed + 1), 0x2000); // original: cmp eax, 0x400



    // Set min and def value of cvar /com_hunkMegs to 512
    patch_int32(ADDR(0x0042c6d6 + 1, 0x080ab9b3 + 4), 512); // minimim value, originally is 1
    patch_int32(ADDR(0x0042c6d1 + 1, 0x080ab9bb + 4), 512); // default value, originally is 160
}