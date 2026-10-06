#include "main/display.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "display.h"
#include "main/display_types.h"
#include "gfx.h"
#include "main/mc.h"
#include "main/mc_types.h"

static const u16 Display_WidthTable[];

static const u16 Display_HeightTable[];

static void Display_SetModeDefault(void);

static const u16 Display_WidthTable[] = {
    0x100,
    0x140,
    0x180,
    0x200,
    0x280,
    0,
};

static const u16 Display_HeightTable[] = {
    0xF0,
    0x1E0,
};

void Display_SetMode(s32 modeBits)
{
    DisplayState* ds;
    u32           widthPixels;
    u32           heightPixels;
    s32           envHeight;
    char          widthStagingByte;
    s32           secondBufferY;
    DisplayState* stateAlias;
    s8            interlaced;
    u32           halfHeight;

    if (!(modeBits & 0xFFFF)) {
        modeBits = DISPLAY_SETUP_DEFAULT;
    }
    interlaced   = (modeBits & DISPLAY_SETUP_INTERLACE_MASK) != 0;
    ds           = &gDisplayState;
    widthPixels  = Display_WidthTable[(u32)(modeBits & DISPLAY_SETUP_WIDTH_MASK) >> 4];
    heightPixels = Display_HeightTable[modeBits & DISPLAY_SETUP_HEIGHT_MASK];
    ds->width    = widthPixels;
    ds->height   = heightPixels;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.interlace != 0) {
        interlaced = 1;
    }
    envHeight     = heightPixels & 0xFFFF;
    ds->interlace = interlaced;
    stateAlias    = ds;
    // A 480-line setup shares one VRAM area; shorter frames alternate vertically.
    if (envHeight != 0x1E0) {
        SetDefDrawEnv(&ds->drawEnv[0], 0, 0, (widthStagingByte = widthPixels, widthPixels & 0xFFFF), envHeight);
        secondBufferY = envHeight + 0x20;
        SetDefDispEnv(&ds->dispEnv[0], 0, secondBufferY, widthPixels & 0xFFFF, envHeight);
        SetDefDrawEnv(&ds->drawEnv[1], 0, secondBufferY, widthPixels & 0xFFFF, envHeight);
        SetDefDispEnv(&ds->dispEnv[1], 0, 0, widthPixels & 0xFFFF, envHeight);
        if (modeBits & DISPLAY_SETUP_RGB24) {
            ds->dispEnv[1].isrgb24 = 1;
            ds->dispEnv[0].isrgb24 = 1;
        } else {
            ds->dispEnv[1].isrgb24 = 0;
            ds->dispEnv[0].isrgb24 = 0;
        }
        gDisplayState.drawEnv[1].ofs[0] = widthPixels >> 1;
        gDisplayState.drawEnv[0].ofs[0] = widthPixels >> 1;
        halfHeight                      = heightPixels >> 1;
        gDisplayState.drawEnv[0].ofs[1] = halfHeight;
        gDisplayState.drawEnv[1].ofs[1] = (heightPixels + halfHeight) + 0x20;
        gDisplayState.drawEnv[1].clip.y = heightPixels + 0x20;
        gDisplayState.drawEnv[1].clip.x = 0;
        gDisplayState.drawEnv[0].clip.x = 0;
        gDisplayState.drawEnv[0].clip.y = 0;
        gDisplayState.drawEnv[1].clip.w = widthPixels;
        gDisplayState.drawEnv[0].clip.w = widthPixels;
        gDisplayState.drawEnv[1].clip.h = heightPixels;
        gDisplayState.drawEnv[0].clip.h = heightPixels;
        gDisplayState.drawEnv[1].dfe    = 1;
        gDisplayState.drawEnv[0].dfe    = 1;
    } else {
        SetDefDrawEnv(&ds->drawEnv[0], 0, 0, (widthStagingByte = widthPixels, widthPixels & 0xFFFF), envHeight);
        SetDefDispEnv(&ds->dispEnv[0], 0, 0, widthPixels & 0xFFFF, envHeight);
        SetDefDrawEnv(&stateAlias->drawEnv[1], 0, 0, widthPixels & 0xFFFF, envHeight);
        SetDefDispEnv(&ds->dispEnv[1], 0, 0, widthPixels & 0xFFFF, envHeight);
        widthStagingByte      = (widthPixels & 0xFFFF) >> 1;
        ds->drawEnv[1].ofs[0] = (widthPixels & 0xFFFF) >> 1;
        ds->drawEnv[0].ofs[0] = (widthPixels & 0xFFFF) >> 1;
        ds->drawEnv[1].ofs[1] = 0xF0;
        ds->drawEnv[0].ofs[1] = 0xF0;
        ds->drawEnv[1].clip.x = 0;
        ds->drawEnv[0].clip.x = 0;
        ds->drawEnv[1].clip.y = 0;
        ds->drawEnv[0].clip.y = 0;
        ds->drawEnv[1].clip.w = widthPixels;
        ds->drawEnv[0].clip.w = widthPixels;
        ds->drawEnv[1].clip.h = heightPixels;
        ds->drawEnv[0].clip.h = heightPixels;
        ds->drawEnv[1].dfe    = 0;
        ds->drawEnv[0].dfe    = 0;
    }
    gDisplayState.drawEnv[1].dtd = 1;
    gDisplayState.drawEnv[0].dtd = 1;
    if (modeBits & DISPLAY_SETUP_NO_CLEAR) {
        gDisplayState.drawEnv[1].isbg = 0;
        gDisplayState.drawEnv[0].isbg = 0;
    } else {
        gDisplayState.drawEnv[1].isbg = 1;
        gDisplayState.drawEnv[0].isbg = 1;
        gDisplayState.drawEnv[1].r0   = 0;
        gDisplayState.drawEnv[0].r0   = 0;
        gDisplayState.drawEnv[1].g0   = 0;
        gDisplayState.drawEnv[0].g0   = 0;
        gDisplayState.drawEnv[1].b0   = 0;
        gDisplayState.drawEnv[0].b0   = 0;
    }
    gDisplayState.dispEnv[1].isinter = interlaced;
    gDisplayState.dispEnv[0].isinter = interlaced;
    if (!(modeBits & DISPLAY_SETUP_KEEP_VIEW)) {
        gfxResetView();
        gfxResetDefaultLights();
    }
}

void displaySetClearColor(s32 red, s32 green, s32 blue)
{
    if (red < 0) {
        gDisplayState.drawEnv[1].isbg = 0;
        gDisplayState.drawEnv[0].isbg = 0;
        return;
    }
    gDisplayState.drawEnv[1].isbg = 1;
    gDisplayState.drawEnv[0].isbg = 1;
    gDisplayState.drawEnv[1].r0   = red;
    gDisplayState.drawEnv[0].r0   = red;
    gDisplayState.drawEnv[1].g0   = green;
    gDisplayState.drawEnv[0].g0   = green;
    gDisplayState.drawEnv[1].b0   = blue;
    gDisplayState.drawEnv[0].b0   = blue;
}

static void Display_SetModeDefault(void)
{
    Display_SetMode(DISPLAY_SETUP_DEFAULT);
}

void displaySetShakeY(s8 offsetY)
{
    s8 clampedY;

    clampedY = offsetY;
    if (offsetY >= DISPLAY_SHAKE_MAX) {
        clampedY = DISPLAY_SHAKE_MAX;
    } else if (offsetY <= DISPLAY_SHAKE_MIN) {
        clampedY = DISPLAY_SHAKE_MIN;
    }
    gDisplayState.shakeY = clampedY;
}
