#include "pe/metabolism.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

/// Visual tuning of the metabolism cast for one Parasite Energy level.
///
/// The cast is a fan of glow wedges, two rings and up to three arcs that
/// brighten and widen around the caster while it throws off sparks. It selects
/// its row with the level digit of the attachment id, less one, which it keeps
/// in `EffectWork::index`.
///
/// The brightness is the cast's `EffectWork::scale`: the green channel of the
/// drawing, which red and blue are shifted down from. The radius is its
/// `EffectWork::angle`: the extent of the fan and the arcs, twice the radius
/// of the rings, and the distance from the caster each spark is spawned at.
typedef struct {
    s16 wedgeCount;      // Glow wedges in the fan, one random angle each; the angle table holds 16
    s16 brightnessLimit; // Brightness the growth stops at; below it the cast gains 0x10 a frame
    s16 radiusStep;      // Radius gained per frame, while the cast grows and while it fades
    s16 radiusLimit;     // Radius that ends the growth; also the sprite radius each spark is spawned with
} _MetabolismLevelTuning;
STATIC_ASSERT_SIZEOF(_MetabolismLevelTuning, 8);

/// Per-level tuning for the metabolism drain, one row per PE level 1-3,
/// weakest first.
static _MetabolismLevelTuning D_metabolism_8012FB54[] = {
    { 0x0008, 0x0080, 0x0020, 0x0400 },
    { 0x000C, 0x00B0, 0x0030, 0x0500 },
    { 0x0010, 0x00E0, 0x0040, 0x0600 },
};

/// The `SndEvt_EnqueueType6` id for each `D_metabolism_8012FB54` row.
static s32 D_metabolism_8012FB6C[] = { 0xE01F0001, 0xE0220001, 0xE0250001 };

/// Scratch angles for the fan, one per wedge: `(i << 10)` plus a 10-bit
/// random offset, seeded by state 0 and swept by `func_metabolism_8012F840`.
static s16 D_metabolism_8012FB78[16];

static void func_metabolism_8012F840(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Runs one frame of the metabolism cast. Cancel (`Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD`
/// or `gRoomEffectState->peEffectControl >= 4`) releases the work block. State 0 parents the
/// coordinate to the player with an identity rotation lifted 0x400 above it,
/// picks the intensity row from the combo counter, seeds one random angle per
/// fan wedge into `D_metabolism_8012FB78`, and plays the combo-indexed cue.
/// State 1 grows brightness and radius, and each frame spins the coordinate to
/// three random yaws, rotating `EffectWork.move` through the new frame and
/// then overwriting it with the `angle` circle at `step`, to parent
/// three `0x60013` sparks; it hands over to state 2 once the radius reaches
/// the row's `radiusLimit`. State 2 shrinks brightness by 0x10 a frame and drops
/// to state 3 - release - below 0x11. States 1 and 2 both draw the fan wedges,
/// two rings and two or three arcs, each arc on a colour halved again from the
/// last.
void func_metabolism_8012EF34(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    EffectWork*       spawned;
    s32               pan;
    s32               bright;
    s32               i;
    s32               temp_lo;
    u8                rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        effectKillTask(mem, arg0);
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            rot                 = (GfxRotationWords*)&coord->coord;
            coord->parent       = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            rot->m00M01         = ONE;
            rot->m02M10         = 0;
            rot->m11M12         = ONE;
            rot->m20M21         = 0;
            rot->m22            = ONE;
            coord->coord.t[0]   = 0;
            coord->coord.t[1]   = -0x400;
            coord->coord.t[2]   = 0;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            arg0->state = 1;
            mem->index  = (Gp_StateC08.attachId % 10) - 1;
            mem->angle  = 0x80;
            {
                s32 rng;

                for (i = 0; i < D_metabolism_8012FB54[mem->index].wedgeCount; i++) {
                    rng                      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_metabolism_8012FB78[i] = (i << 10) + (((u32)rng >> 16) & 0x3FF);
                    gRandomLcgState          = rng;
                }
            }
            Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
            pan                = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(D_metabolism_8012FB6C[mem->index], pan,
                                (s8)worldCoordGetOriginAudioDepth(coord));
            /* fallthrough */
        case 1:
            actorRenderComposeCoord(coord);
            bright = mem->scale;
            if (bright < D_metabolism_8012FB54[mem->index].brightnessLimit) {
                bright += 0x10;
            }
            mem->scale = bright;
            mem->angle = mem->angle + D_metabolism_8012FB54[mem->index].radiusStep;
            {
                s32 rng;
                s32 rng2;

                for (i = 0; i < 3; i++) {
                    rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    rng2            = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState = rng;
                    mem->step       = ((u32)rng >> 16) & 0xFFF;
                    gRandomLcgState = rng2;
                    gfxRotMatrixY(&coord->coord, ((u32)rng2 >> 16) & 0xFFF, 0);
                    gte_SetRotMatrix(&coord->coord);
                    gte_ldv0(&mem->move);
                    gte_rtv0();
                    gte_stsv(&mem->move);
                    mem->move.vx = (rcos(mem->step) * mem->angle) >> 12;
                    temp_lo      = rsin(mem->step) * mem->angle;
                    mem->move.vz = 0;
                    mem->move.vy = temp_lo >> 12;
                    spawned      = Gp_SpawnEff(EFFECT_METABOLISM_SPARKLE, coord,
                                               (s32)D_metabolism_8012FB54[mem->index].radiusLimit,
                                               &mem->move);
                    if (spawned != NULL) {
                        taskReparent(arg0, spawned->task);
                    }
                }
            }
            if (mem->angle >= D_metabolism_8012FB54[mem->index].radiusLimit) {
                arg0->state = 2;
            }
            for (i = 0; i < D_metabolism_8012FB54[mem->index].wedgeCount; i++) {
                func_metabolism_8012F840(coord, mem->angle, D_metabolism_8012FB78[i],
                                         mem->scale);
            }
            goto draw;
        case 2:
            actorRenderComposeCoord(coord);
            for (i = 0; i < D_metabolism_8012FB54[mem->index].wedgeCount; i++) {
                func_metabolism_8012F840(coord, mem->angle, D_metabolism_8012FB78[i],
                                         mem->scale);
            }
            mem->scale = mem->scale - 0x10;
            mem->angle = mem->angle + D_metabolism_8012FB54[mem->index].radiusStep;
            if (mem->scale < 0x11) {
                arg0->state = 3;
            }
        draw:
            rgb[0] = mem->scale >> 2;
            rgb[1] = (u8)mem->scale;
            rgb[2] = mem->scale >> 1;
            Gp_DrawRing(coord, mem->angle >> 1, rgb);
            Gp_DrawRing(coord, mem->angle >> 1, rgb);
            rgb[0] >>= 1;
            rgb[1] >>= 1;
            rgb[2] >>= 1;
            Gp_DrawArc(coord, mem->angle, 0x80, rgb);
            if (mem->age & 1) {
                rgb[1] >>= 1;
                rgb[2] <<= 1;
                Gp_DrawArc(coord, 0x80, mem->angle, rgb);
            }
            if (mem->index != 0) {
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                Gp_DrawArc(coord, (s16)(mem->angle + 0x200), 0x80, rgb);
            }
            return;
        case 3:
            effectKillTask(mem, arg0);
            return;
    }
}

/// Metabolism billboard. State 0 seeds the spin from the spawn argument and
/// picks the draw path: the plain additive quad (state 1), or, one roll in
/// three when the level's difficulty band allows it, the alternate
/// `func_800EB6E8` quad that fades its colour by 0x18 a frame (state 2).
/// Both states lift the frame and draw on odd ticks until it runs out.
void func_metabolism_8012F5A0(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         y;
    s16         step;
    u16         kind;
    u16         roll;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->move.vx = 0;
            mem->move.vy = 8;
            mem->move.vz = 0;
            mem->angle   = arg0->spawnArg1.value & 0xFFF;
            kind         = Gp_StateC08.attachId % 10U;
            if (kind - 1 < 2 ||
                (gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT,
                 roll            = (gRandomLcgState >> 16) % 3U, roll != 0)) {
                arg0->state     = 1;
                mem->period     = 0x1000;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
            } else {
                arg0->state = 2;
                mem->scale  = 0xC0;
                mem->period = 0x3000;
            }
            return;
        case 1:
            step                = mem->move.vy;
            y                   = coord->coord.t[1] + step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            actorRenderComposeCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            if (mem->index < 8) {
                if (mem->age & 1) {
                    Gp_DrawFxQuad(coord, mem->index, mem->angle,
                                  mem->scale | mem->period);
                    return;
                }
            } else {
                effectKillTask(mem, arg0);
                return;
            }
            break;
        case 2:
            step                = mem->move.vy;
            y                   = coord->coord.t[1] + step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            actorRenderComposeCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            if (mem->index < 8) {
                if (mem->age & 1) {
                    func_800EB6E8(coord, mem->index, mem->angle,
                                  mem->scale | mem->period);
                    mem->scale = mem->scale - 0x18;
                    return;
                }
            } else {
                effectKillTask(mem, arg0);
                return;
            }
            break;
    }
}

/// Draws one wedge of the metabolism fan as a Gouraud triangle. `arg0`'s
/// origin is projected once through `GsWSMATRIX`; the two outer corners sit
/// `arg1` screen units away at `arg2 - 0x20` and `arg2 + 0x20`. Apex colour
/// is a single channel: red is halved, green is `arg3`, blue is shifted by
/// the low bit of `gDisplayState.animFrame`. The rim fades to black. A
/// negative `gte_stflg` drops the wedge.
static void func_metabolism_8012F840(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    u8*                  head;
    EffectCentreScratch* block;
    SVECTOR*             vec;
    POLY_G3*             prim;
    s32                  ang;
    s32                  ang2;
    u16                  vz;

    head                                                                        = SCRATCH_STACK_CURSOR(u8);
    ((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->worldPoint.vx = (u16)arg0->workm.t[0];
    block                                                                       = (EffectCentreScratch*)(head - sizeof(EffectCentreScratch));
    block->worldPoint.vy                                                        = (u16)arg0->workm.t[1];
    vz                                                                          = (u16)arg0->workm.t[2];
    SCRATCH_STACK_CURSOR(EffectCentreScratch)                                   = block;
    block->worldPoint.vz                                                        = vz;
    vec                                                                         = &block->worldPoint;
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(vec);
    gte_rtps();
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyG3(prim);
        setRGB0(prim, arg3 >> 1, arg3, arg3 >> (gDisplayState.animFrame & 1));
        setRGB1(prim, 0, 0, 0);
        setRGB2(prim, 0, 0, 0);
        block->screenExtent = (arg1 * 128) / block->depth;
        ang                 = arg2;
        ang2                = ang - 0x20;
        prim->x0            = block->screenX;
        prim->y0            = block->screenY;
        prim->x1            = block->screenX + ((block->screenExtent * rsin(ang2)) >> 12);
        prim->y1            = block->screenY + ((block->screenExtent * rcos(ang2)) >> 12);
        ang                += 0x20;
        prim->x2            = block->screenX + ((block->screenExtent * rsin(ang)) >> 12);
        prim->y2            = block->screenY + ((block->screenExtent * rcos(ang)) >> 12);
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
        gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, block->depth);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}
