#include "drawing.h"

#include "shared.h"
#include "../shared/cod2_dvars.h"
#include "../shared/cod2_client.h"
#include "radar.h"

vec4_t colWhite			    = { 1, 1, 1, 1 };
vec4_t colBlack			    = { 0, 0, 0, 1 };
vec4_t colRed			    = { 1, 0, 0, 1 };
vec4_t colGreen			    = { 0, 1, 0, 1 };
vec4_t colBlue			    = { 0, 0, 1, 1 };
vec4_t colYellow		    = { 1, 1, 0, 1 };


dvar_t* cg_drawSpectatedPlayerName = NULL;
dvar_t* cg_drawCompass = NULL;
dvar_t* cg_hudCompassOffsetX = NULL;
dvar_t* cg_hudCompassOffsetY = NULL;

/**
 * Drawing of the text "following" and player name in top center of the screen when spectating.
 */
int CG_DrawFollow() {

    if (!cg_drawSpectatedPlayerName->value.boolean) {
        return 0;
    }

    int drawed = 0;
    ASM_CALL(RETURN(drawed), 0x004cba90);
    return drawed;
}


struct compass_hud_data
{
    float x;
    float y;
    float w;
    float h;
    horizontalAlign_e horizontalAlign;
    verticalAlign_e verticalAlign;
};

/** Drawing of the rotating image of compass */
void CG_DrawPlayerCompass(void* shader, vec4_t* color) {
    compass_hud_data* data; ASM__movr(data, "esi");

    if (!cg_drawCompass->value.boolean)
        return;

    data->x += cg_hudCompassOffsetX->value.decimal;
    data->y += cg_hudCompassOffsetY->value.decimal;

    ASM_CALL(RETURN_VOID, 0x004c5400, 2, ESI(data), PUSH(shader), PUSH(color));
}

/** Drawing of the objectives on the compass. */
void CG_DrawPlayerCompassObjectives(compass_hud_data* data, vec4_t* color) {
    
    if (!cg_drawCompass->value.boolean)
        return;

    data->x += cg_hudCompassOffsetX->value.decimal;
    data->y += cg_hudCompassOffsetY->value.decimal;

    ASM_CALL(RETURN_VOID, 0x004c5620, 2, PUSH(data), PUSH(color));
}

/** Drawing of the players on the compass. */
void CG_DrawCompassFriendlies(compass_hud_data* data, vec4_t* color) {
    if (!cg_drawCompass->value.boolean)
        return;
    
    data->x += cg_hudCompassOffsetX->value.decimal;
    data->y += cg_hudCompassOffsetY->value.decimal;

    ASM_CALL(RETURN_VOID, 0x004dafe0, 2, PUSH(data), PUSH(color));
}

/** Draws the background for the compass. */
void __cdecl CG_DrawPlayerCompassBack(void* shader, vec4_t* color) {
    compass_hud_data* data; ASM__movr(data, "esi");

    radar_draw();

    if (!cg_drawCompass->value.boolean)
        return;

    data->x += cg_hudCompassOffsetX->value.decimal;
    data->y += cg_hudCompassOffsetY->value.decimal;

    ASM_CALL(RETURN_VOID, 0x004c5510, 2, ESI(data), PUSH(shader), PUSH(color));
}


void CG_DrawCrosshairNames() {
    ASM_CALL(RETURN_VOID, 0x004c97c0);
}


// Help web page removed, fixed crash when getting translations
void Sys_DirectXFatalError() {
    MessageBoxA(NULL, "DirectX(R) encountered an unrecoverable error.", "DirectX Error", MB_OK | MB_ICONERROR);
    ExitProcess(-1);
}



/** Called every frame on frame start. */
void drawing_frame() {
}

/** Called only once on game start after common inicialization. Used to initialize variables, cvars, etc. */
void drawing_init() {
    cg_drawSpectatedPlayerName = Dvar_RegisterBool("cg_drawSpectatedPlayerName", true, (enum dvarFlags_e)(DVAR_CHANGEABLE_RESET));
    cg_drawCompass = Dvar_RegisterBool("cg_drawCompass", true, (enum dvarFlags_e)(DVAR_CHANGEABLE_RESET));
    cg_hudCompassOffsetX = Dvar_RegisterFloat("cg_hudCompassOffsetX", 0.0f, -640.0f, 640.0f, (enum dvarFlags_e)(DVAR_CHANGEABLE_RESET));
    cg_hudCompassOffsetY = Dvar_RegisterFloat("cg_hudCompassOffsetY", 0.0f, -480.0f, 480.0f, (enum dvarFlags_e)(DVAR_CHANGEABLE_RESET));
}

/** Called before the entry point is called. Used to patch the memory. */
void drawing_patch() {
    patch_call(0x004cbdce, (unsigned int)CG_DrawFollow);

    patch_call(0x004c6870, (unsigned int)CG_DrawPlayerCompass);
    patch_call(0x004c6884, (unsigned int)CG_DrawPlayerCompassBack);
    patch_call(0x004c6898, (unsigned int)CG_DrawPlayerCompassObjectives);
    patch_call(0x004c68e8, (unsigned int)CG_DrawCompassFriendlies);
    patch_call(0x004cbd36, (unsigned int)CG_DrawCrosshairNames);
    patch_call(0x004cbd6b, (unsigned int)CG_DrawCrosshairNames);

    // Improve DirectX error message
    patch_int32(0x0040fcf5 + 4, (unsigned int)Sys_DirectXFatalError);
}
