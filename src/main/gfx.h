#ifndef MAIN_PRIVATE_GFX_H
#define MAIN_PRIVATE_GFX_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

/// Color/light matrix written by Gfx_SetDefaultFlatLight / Gfx_SetLightAmbient.
extern MATRIX D_80074080;

void Gfx_StoreImageSlot(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

void Gfx_LoadImageSlot(s32 arg0, s32 arg1, s32 arg2);

void Gfx_InitCoordinateTrees(void);

void Gpu_InitDefaultLights(void);

#endif // MAIN_PRIVATE_GFX_H
