#include "gsc.h"

#include <stdarg.h> // va_list, va_start, va_end

#include "shared.h"
#include "gsc_test.h"
#include "cod2_common.h"
#include "cod2_script.h"
#include "cod2_math.h"
#include "cod2_server.h"


// Array of custom script functions
scr_function_t scriptFunctions[] = {

	#if DEBUG
	{"test_returnUndefined", gsc_test_returnUndefined, 0},
	{"test_returnBool", gsc_test_returnBool, 0},
	{"test_returnInt", gsc_test_returnInt, 0},
	{"test_returnFloat", gsc_test_returnFloat, 0},
	{"test_returnString", gsc_test_returnString, 0},
	{"test_returnVector", gsc_test_returnVector, 0},
	{"test_returnArray", gsc_test_returnArray, 0},
	{"test_getAll", gsc_test_getAll, 0},
	{"test_allOk", gsc_test_allOk, 0},
	#endif

	{NULL, NULL, 0}
};

// Array of custom methods
scr_method_t scriptMethods[] =
{
	#if DEBUG
	{"test_playerGetName", gsc_test_playerGetName, 0},
	#endif

	{NULL, NULL, 0} // Terminator
};

// Array of custom callbacks
callback_t callbacks[] =
{
	#if DEBUG
	{ &codecallback_test, 			"_callback_tests", "test_func", true},
	{ &codecallback_test_player, 	"_callback_tests", "test_onPlayerConnect", true},
	#endif
};

// This function is called when scripts are being compiled and function names are being resolved.
xfunction_t Scr_GetCustomFunction(const char **fname, int *fdev)
{
	// Try to find original function
	xfunction_t m = Scr_GetFunction(fname, fdev);
	if ( m )
		return m;

	// Try to find new custom function
	for ( int i = 0; scriptFunctions[i].name; i++ )
	{
		if ( strcasecmp(*fname, scriptFunctions[i].name) )
			continue;

		scr_function_t func = scriptFunctions[i];
		*fname = func.name;
		*fdev = func.developer;
		return func.call;
	}

	return NULL;
}

// This function is called when scripts are being compiled and method names are being resolved.
xmethod_t Scr_GetCustomMethod(const char **fname, qboolean *fdev)
{
	// Try to find original method
	xmethod_t m = Scr_GetMethod(fname, fdev);
	if ( m )
		return m;

	// Try to find new custom method
	for ( int i = 0; scriptMethods[i].name; i++ )
	{
		if ( strcasecmp(*fname, scriptMethods[i].name) )
			continue;

		scr_method_t func = scriptMethods[i];

		*fname = func.name;
		*fdev = func.developer;

		return func.call;
	}

	return NULL;
}

// Called when CodeCallback_PlayerConnect is called
void gsc_onPlayerConnect(int entnum) {
	gsc_test_onPlayerConnect(entnum);
}
short CodeCallback_PlayerConnect_Win32(int entnum, int classnum, int paramcount) {
	int handle; ASM( movr, handle, "eax" );
	gsc_onPlayerConnect(entnum);
	short ret;
	ASM_CALL(RETURN(ret), 0x00482190, 3, EAX(handle), PUSH(entnum), PUSH(classnum), PUSH(paramcount));
	return ret;
}
void CodeCallback_PlayerConnect_Linux(gentity_t *ent) {
	gsc_onPlayerConnect(ent->s.number);
	ASM_CALL(RETURN_VOID, 0x08118350, 1, PUSH(ent));
}


// Called when CodeCallback_StartGameType is called
void gsc_onStartGameType() {
	gsc_test_onStartGameType();
}
short CodeCallback_StartGameType_Win32(int paramcount) {
	int handle; ASM( movr, handle, "eax" );
	gsc_onStartGameType();
	short ret;
	ASM_CALL(RETURN(ret), 0x00482080, 1, EAX(handle), PUSH(paramcount));
	return ret;
}
void CodeCallback_StartGameType_Linux() {
	gsc_onStartGameType();
	ASM_CALL(RETURN_VOID, 0x08118322);
}


// Loading callback handles from scripts
void GScr_LoadGameTypeScript() {
	// Load original callbacks
	ASM_CALL(RETURN_VOID, ADDR(0x00503f90, 0x08110286), 0);

	// Load new custom callbacks
	for (size_t i = 0; i < sizeof(callbacks)/sizeof(callbacks[0]); i++)
	{
		callback_t *cb = &callbacks[i];
		*cb->variable = Scr_GetFunctionHandle(cb->scriptName, cb->functionName, cb->isNeeded);
	}
}


void gsc_frame() {
}

/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void gsc_init() {
}

/** Called before the entry point is called. Used to patch the memory. */
void gsc_patch()
{
    patch_call(ADDR(0x46E7BF, 0x08070BE7), (int)Scr_GetCustomFunction);
    patch_call(ADDR(0x46EA03, 0x08070E0B), (int)Scr_GetCustomMethod);
	patch_call(ADDR(0x5043fa, 0x0811048c), (int)GScr_LoadGameTypeScript);

	WL(
		patch_call(0x004fc79a, (unsigned int)CodeCallback_StartGameType_Win32),
		patch_call(0x08109499, (unsigned int)CodeCallback_StartGameType_Linux);
	);
	WL(
		patch_call(0x004fe43a, (unsigned int)CodeCallback_PlayerConnect_Win32),
		patch_call(0x080f9091, (unsigned int)CodeCallback_PlayerConnect_Linux);
	);
}