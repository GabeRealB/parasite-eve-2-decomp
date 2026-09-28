#ifndef GAMEPLAY_ROOM_EFFECTS_H
#define GAMEPLAY_ROOM_EFFECTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/effects.h"
#include "gameplay/scene.h"

#include "main/coord.h"
#include "main/task_types.h"

struct GpEffWork;

/// Shared State1C work.
extern s32 D_80115720;

extern s32 D_80115724;

extern s32 D_80115728;

extern s32 D_8011572C;

extern s32 D_80115730;

extern s32 D_80115734;

extern s32 D_80115738;

extern s32 D_8011573C;

extern GpState1C* Gp_State1C;

extern s32 D_80115744;

extern s32 D_8011574C;

extern s32 D_80115750;

extern s32 D_80115754;

extern s32 D_80115758;

extern TaskDesc D_8010FC2C[];

s32 Gp_TraceGroundCoord(GpCoord* arg0, GpCoord* arg1);

s32 func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1);

s32 func_800EA318(s16 arg0, s16 arg1, s16 arg2);

void func_800EA3A0(s32 arg0);

/// Spawns a `GpState1C` effect task and its `GpEffWork` (`memCalloc(0x2C)`).
/// `arg0` packs the `Task_Spawn` bank in bits 16..30 and the type in the low
/// 16 bits; a negative `arg0` bypasses the 0x80 live-effect cap in
/// `GpState1C::effectCount`. `arg1` is the parent coordinate (`NULL` = world):
/// the task's own `TmdObject::coords` coordinate is seeded from it and
/// re-parented to `gGfxViewCoord`. `arg2` becomes `Task::spawnArg1`; `arg3` is an
/// optional offset vector (`NULL` = zero) rotated into the parent's space and
/// kept in `GpEffWork::field_C`. Returns the work object, or `NULL`.
struct GpEffWork* Gp_SpawnEff(s32 arg0, GpCoord* arg1, s32 arg2, SVECTOR* arg3);

/// Full-screen semi-trans POLY_F4. `arg0` is RGB; `arg1` is ABR (low 2 bits).
void Gp_DrawFadeQuad(u8* arg0, s32 arg1);

/// Handwritten GTE routine. Draws a textured sprite at `arg0`; `arg1` is a
/// signed half-extent, `arg2` a scale, and `arg3` the RGB triple.
void Gp_DrawArc(GpCoord* arg0, s32 arg1, s32 arg2, u8* arg3);

/// Handwritten GTE routine. Draws an eight-segment gouraud ring centred on
/// `arg0`'s world position: `arg1` is the radius in world units (scaled by
/// 64 and divided by the projected OTZ) and `arg2` the RGB triple, which
/// only lights the ring's inner vertex so each `POLY_G4` fades to black.
void Gp_DrawRing(GpCoord* arg0, s32 arg1, u8* arg2);

/// Draws one textured, additive `POLY_FT4` billboard at `arg0`'s projected
/// position. `arg1` is the animation frame (U origin `arg1 * 32`, the sprite
/// is 0x20 x 0x20 at V 0x18), `arg2` the radius (scaled by 31 and divided by
/// the projected OTZ) and `arg3` packs the sprite's CLUT index
/// (`Gp_QuadClutX`) in its top nibble and the spin angle in its low 12 bits,
/// so the quad's corners sit at `angle` and `angle + 0x400`.
void Gp_DrawFxQuad(GpCoord* arg0, u16 arg1, s16 arg2, u16 arg3);

/// Draws a grayscale, semitransparent textured billboard at the projected
/// world position. `arg1 & 3` selects the 24-pixel animation frame, `arg2`
/// packs the texture bank in its top nibble and radius in its low 12 bits,
/// and `arg3` packs the CLUT index in its top nibble and brightness in its
/// low byte.
void func_800EB6E8(GpCoord* arg0, u16 arg1, u16 arg2, u16 arg3);

void Gp_DrawBand(GpCoord* arg0, s16 arg1, u8* arg2);

void Gp_DrawBandEx(GpCoord* arg0, s16 arg1, s32 arg2, u8* arg3);

void Gp_ReleaseState1CMem(void* arg0, Task* arg1);

void Gp_PulseState1C(void);

void Gp_AddTpageShift(P_TAG* arg0, s32 arg1, s32 arg2);

#endif // GAMEPLAY_ROOM_EFFECTS_H
