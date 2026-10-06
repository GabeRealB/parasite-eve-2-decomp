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

/// Gap in VRAM rows between the two 240-line framebuffer regions.
enum { DISPLAY_FRAMEBUFFER_VERTICAL_GAP = 32 };

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

/// Initializes two environments that draw and display opposite VRAM regions.
static inline void _displayInitializeAlternatingFramebuffers(DisplayState* display, u32 widthPixels, s32 heightPixels)
{
    s32 lowerFramebufferY;

    SetDefDrawEnv(&display->drawEnv[0], 0, 0, (u16)widthPixels, heightPixels);
    lowerFramebufferY = heightPixels + DISPLAY_FRAMEBUFFER_VERTICAL_GAP;
    SetDefDispEnv(&display->dispEnv[0], 0, lowerFramebufferY, (u16)widthPixels, heightPixels);
    SetDefDrawEnv(&display->drawEnv[1], 0, lowerFramebufferY, (u16)widthPixels, heightPixels);
    SetDefDispEnv(&display->dispEnv[1], 0, 0, (u16)widthPixels, heightPixels);
}

/// Initializes two environments that draw and display the same VRAM region.
static inline void _displayInitializeSharedFramebuffers(DisplayState* display, u32 widthPixels, s32 heightPixels)
{
    SetDefDrawEnv(&display->drawEnv[0], 0, 0, (u16)widthPixels, heightPixels);
    SetDefDispEnv(&display->dispEnv[0], 0, 0, (u16)widthPixels, heightPixels);
    SetDefDrawEnv(&display->drawEnv[1], 0, 0, (u16)widthPixels, heightPixels);
    SetDefDispEnv(&display->dispEnv[1], 0, 0, (u16)widthPixels, heightPixels);
}

void displayConfigureFramebuffers(s32 setupBits)
{
    enum {
        DISPLAY_SETUP_BITS_MASK         = 0xFFFF,
        DISPLAY_FRAMEBUFFER_FULL_HEIGHT = 480,
        DISPLAY_FRAMEBUFFER_HALF_HEIGHT = 240,
    };

    DisplayState* display;
    u32           widthPixels;
    u32           heightPixels;
    s32           envHeightPixels;
    s8            interlaced;
    u32           halfHeightPixels;

    if (!(setupBits & DISPLAY_SETUP_BITS_MASK)) {
        setupBits = DISPLAY_SETUP_DEFAULT;
    }
    interlaced      = (setupBits & DISPLAY_SETUP_INTERLACE_MASK) != 0;
    display         = &gDisplayState;
    widthPixels     = Display_WidthTable[(setupBits & DISPLAY_SETUP_WIDTH_MASK) >> 4];
    heightPixels    = Display_HeightTable[setupBits & DISPLAY_SETUP_HEIGHT_MASK];
    display->width  = widthPixels;
    display->height = heightPixels;
    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.interlace != 0) {
        interlaced = 1;
    }
    envHeightPixels    = (u16)heightPixels;
    display->interlace = interlaced;
    // Separate 240-line regions alternate; a 480-line frame occupies both fields.
    if (envHeightPixels != DISPLAY_FRAMEBUFFER_FULL_HEIGHT) {
        _displayInitializeAlternatingFramebuffers(display, widthPixels, envHeightPixels);
        if (setupBits & DISPLAY_SETUP_RGB24) {
            display->dispEnv[1].isrgb24 = 1;
            display->dispEnv[0].isrgb24 = 1;
        } else {
            display->dispEnv[1].isrgb24 = 0;
            display->dispEnv[0].isrgb24 = 0;
        }
        gDisplayState.drawEnv[1].ofs[0] = widthPixels >> 1;
        gDisplayState.drawEnv[0].ofs[0] = widthPixels >> 1;
        halfHeightPixels                = heightPixels >> 1;
        gDisplayState.drawEnv[0].ofs[1] = halfHeightPixels;
        gDisplayState.drawEnv[1].ofs[1] = (heightPixels + halfHeightPixels) + DISPLAY_FRAMEBUFFER_VERTICAL_GAP;
        gDisplayState.drawEnv[1].clip.y = heightPixels + DISPLAY_FRAMEBUFFER_VERTICAL_GAP;
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
        // Both environments share VRAM; the SDK defaults keep 16-bit display.
        _displayInitializeSharedFramebuffers(display, widthPixels, envHeightPixels);
        display->drawEnv[1].ofs[0] = (u16)widthPixels >> 1;
        display->drawEnv[0].ofs[0] = (u16)widthPixels >> 1;
        display->drawEnv[1].ofs[1] = DISPLAY_FRAMEBUFFER_HALF_HEIGHT;
        display->drawEnv[0].ofs[1] = DISPLAY_FRAMEBUFFER_HALF_HEIGHT;
        display->drawEnv[1].clip.x = 0;
        display->drawEnv[0].clip.x = 0;
        display->drawEnv[1].clip.y = 0;
        display->drawEnv[0].clip.y = 0;
        display->drawEnv[1].clip.w = widthPixels;
        display->drawEnv[0].clip.w = widthPixels;
        display->drawEnv[1].clip.h = heightPixels;
        display->drawEnv[0].clip.h = heightPixels;
        display->drawEnv[1].dfe    = 0;
        display->drawEnv[0].dfe    = 0;
    }
    // Clear and display flags apply to both environment pairs.
    gDisplayState.drawEnv[1].dtd = 1;
    gDisplayState.drawEnv[0].dtd = 1;
    if (setupBits & DISPLAY_SETUP_NO_CLEAR) {
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
    if (!(setupBits & DISPLAY_SETUP_KEEP_VIEW)) {
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

/// Restores the default 320x240 environments, black clearing, view and lights.
///
/// The live save's interlace preference still applies.
static void _displayConfigureDefaultFramebuffers(void)
{
    displayConfigureFramebuffers(DISPLAY_SETUP_DEFAULT);
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
