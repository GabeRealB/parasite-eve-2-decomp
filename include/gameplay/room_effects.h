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

/// Shared State1C work.
extern s32 D_80115720;

extern s32 D_80115724;

extern s32 D_80115728;

extern s32 D_8011572C;

extern s32 D_80115730;

extern s32 D_80115734;

extern s32 D_80115738;

extern s32 D_8011573C;

/// The room-effect controller's live `RoomEffectState`.
///
/// Allocated on the primary heap when the controller starts and stored both
/// here and as that task's work. Gameplay and the room, actor, weapon and PE
/// overlays borrow it for the controller's lifetime. Callers do not test it
/// for `NULL`, and destroying the task does not clear it.
extern RoomEffectState* gRoomEffectState;

extern s32 D_80115744;

extern s32 D_8011574C;

extern s32 D_80115750;

extern s32 D_80115754;

extern s32 D_80115758;

extern TaskDesc D_8010FC2C[];

s32 Gp_TraceGroundCoord(GfxCoord* arg0, GfxCoord* arg1);

s32 func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1);

s32 func_800EA318(s16 arg0, s16 arg1, s16 arg2);

void func_800EA3A0(s32 arg0);

/// Spawns a counted effect task and its `EffectWork`.
/// `arg0` packs the `Task_Spawn` bank in bits 16..30 and the type in the low
/// 16 bits; a negative `arg0` bypasses the ordinary spawn limit (129) in
/// `RoomEffectState::effectCount`. `arg1` is stored in `EffectWork::parent`;
/// NULL stores the view coordinate there. The effect's own coordinate is
/// parented to `gGfxViewCoord` either way, after the offset is rotated into
/// it. `arg2` becomes `Task::spawnArg1`. `arg3` is an optional offset: NULL is
/// read as a zero vector, the components are copied into `EffectWork::pos`,
/// and the original pointer, NULL included, is stored in `EffectWork::field_C`.
/// Returns the work object, or `NULL`.
EffectWork* Gp_SpawnEff(s32 arg0, GfxCoord* arg1, TaskSpawnArg arg2, SVECTOR* arg3);

/// Full-screen semi-trans POLY_F4. `arg0` is RGB; `arg1` is ABR (low 2 bits).
void Gp_DrawFadeQuad(u8* arg0, s32 arg1);

/// Handwritten GTE routine. Draws a textured sprite at `arg0`; `arg1` is a
/// signed half-extent, `arg2` a scale, and `arg3` the RGB triple.
void Gp_DrawArc(GfxCoord* arg0, s32 arg1, s32 arg2, u8* arg3);

/// Handwritten GTE routine. Draws an eight-segment gouraud ring centred on
/// `arg0`'s world position: `arg1` is the radius in world units (scaled by
/// 64 and divided by the projected OTZ) and `arg2` the RGB triple, which
/// only lights the ring's inner vertex so each `POLY_G4` fades to black.
void Gp_DrawRing(GfxCoord* arg0, s32 arg1, u8* arg2);

/// Draws one textured, additive `POLY_FT4` billboard at `arg0`'s projected
/// position. `arg1` is the animation frame (U origin `arg1 * 32`, the sprite
/// is 0x20 x 0x20 at V 0x18), `arg2` the radius (scaled by 31 and divided by
/// the projected OTZ) and `arg3` packs the sprite's CLUT index
/// (`Gp_QuadClutX`) in its top nibble and the spin angle in its low 12 bits,
/// so the quad's corners sit at `angle` and `angle + 0x400`.
void Gp_DrawFxQuad(GfxCoord* arg0, u16 arg1, s16 arg2, u16 arg3);

/// Draws a grayscale, semitransparent textured billboard at the projected
/// world position. `arg1 & 3` selects the 24-pixel animation frame, `arg2`
/// packs the texture bank in its top nibble and radius in its low 12 bits,
/// and `arg3` packs the CLUT index in its top nibble and brightness in its
/// low byte.
void func_800EB6E8(GfxCoord* arg0, u16 arg1, u16 arg2, u16 arg3);

void Gp_DrawBand(GfxCoord* arg0, s16 arg1, u8* arg2);

void Gp_DrawBandEx(GfxCoord* arg0, s16 arg1, s32 arg2, u8* arg3);

void Gp_ReleaseState1CMem(void* arg0, Task* arg1);

void Gp_PulseState1C(void);

/// GPU semitransparency modes, with B the background and F the primitive colour.
enum {
    GPU_BLEND_AVERAGE = 0, // B/2 + F/2
    /// GPU semitransparency selector for full-strength foreground + framebuffer colour.
    ///
    /// RGB channels saturate at their maximum. Pass this unshifted ABR value
    /// to `gpuSetPrimitiveBlendMode` or `getTPage`; the packet stores it in
    /// bits 5..6. The primitive must also have semitransparency enabled.
    GPU_BLEND_ADD         = 1,
    GPU_BLEND_SUBTRACT    = 2, // B - F
    GPU_BLEND_ADD_QUARTER = 3, // B + F/4
};

/// Enables semitransparency and queues the blend mode for an untextured primitive.
///
/// `primitive` must be an initialized, writable polygon, line or rectangle packet
/// already linked into the current depth ordering table at the same `depth`.
/// `depth` is the sorting depth before `gDisplayState.otDepthShift`, not a tag
/// index; the scaled value wraps to one of the 1024 depth tags. The low two bits
/// of `blendMode` select a `GPU_BLEND_*` mode.
///
/// Consumes `sizeof(DR_TPAGE)` bytes from `gGpuPrimCursor` without a capacity
/// check and prepends that command at the depth slot, ahead of the primitive.
/// It enables dithering, prohibits drawing into the displayed area and selects
/// the fixed 4-bit texture page at (640, 0). The draw mode remains active until
/// replaced. Both packets borrow the frame arena until GPU drawing completes.
void gpuSetPrimitiveBlendMode(void* primitive, s32 blendMode, s32 depth);

#endif // GAMEPLAY_ROOM_EFFECTS_H
