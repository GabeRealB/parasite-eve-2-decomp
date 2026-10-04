#include "pe/apobiosis.h"

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
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/glow_draw.h"

/// Size and pace of the apobiosis effect at one Parasite Energy level.
///
/// The cast and every shard it spawns select a row by the level digit of the
/// spell being cast (`AttachmentState::attachId % 10 - 1`), so a higher level
/// radiates more strips, faster and larger. A strip is a textured quad laid
/// between two projected points; a sprite is a camera-facing animated quad.
///
/// A scale sizes its primitive before the perspective divide: the on-screen
/// half-extent in pixels is the scale times the texture's extent in texels,
/// over the view depth.
typedef struct {
    s16 stripCount;        // Strips the cast radiates each frame; it seeds two angles for each
    s16 playerSpriteScale; // Scale of the sprite the cast draws on the player
    s16 radiusStep;        // Added each frame to the cast's radius, which is its halo's size and its strips' length. Also the distance from a strip's first angle to its second in the angle table
    u16 stripScale;        // Scale of every strip, and of a shard's own sprite; a shard pinned to its coordinate doubles it
} _ApobiosisLevelParams;
STATIC_ASSERT_SIZEOF(_ApobiosisLevelParams, 0x8);

static void func_apobiosis_8012F808(s16 bright);

/// Per-level tuning for the apobiosis pulse, one row per PE level 1-3,
/// weakest first.
static _ApobiosisLevelParams D_apobiosis_80130B5C[] = {
    { 0x0004, 0x0400, 0x00C0, 0x0280 },
    { 0x0006, 0x0500, 0x0100, 0x0300 },
    { 0x0008, 0x0600, 0x0140, 0x0400 },
};

/// The `sndEvtRequestScriptStart` id the cast plays, one per `D_apobiosis_80130B5C`
/// row, so the sound follows the cast's level like the burst does.
static s32 D_apobiosis_80130B74[] = { 0xE0170001, 0xE01A0001, 0xE01D0001 };

static void func_apobiosis_8013017C(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_apobiosis_80130630(GfxCoord* arg0, SVECTOR* arg1, s16 arg2, s16 arg3);

/// Ring azimuths, two rows of up to eight. `func_apobiosis_8012EF4C` lays out
/// `_ApobiosisLevelParams::stripCount * 2` of them at `(i << 10) + rand()` in state 0 and
/// then jitters each by +-0x80 a frame; the first row is the shard's own angle
/// and the row `_ApobiosisLevelParams::radiusStep` entries later is its elevation.
static s16 D_apobiosis_80130B80[16];

/// The running cast task, cached by `func_apobiosis_8012EF4C` so each shard
/// can reparent itself onto the cast when it starts.
static Task* D_apobiosis_80130BA0;

/// The apobiosis cast. Six states drive one screen flash plus a growing ring
/// of shards, scaled by `D_apobiosis_80130B5C[Gp_StateC08.attachId % 10 - 1]`
/// so a longer combo casts a wider burst. State 0 parents the effect
/// coordinate on `EffectWork.parent` at the origin, publishes the task in
/// `D_apobiosis_80130BA0` so every shard can reparent onto it, plays the row's
/// `sndEvtRequestScriptStart` id panned at the coordinate, and seeds
/// `D_apobiosis_80130B80` with `stripCount * 2` angles - the ring's two rows of
/// azimuths. State 1 flashes at `step`, drags the coordinate down 0x400,
/// grows `scale` by the row's `radiusStep` each frame and redraws both the
/// player's ring and the shard ring, jittering every angle by +-0x80 per frame.
/// States 2..4 fade the flash out at 0x10 / 0xC / 8 a frame while spawning
/// 0x600F7 sparks on random polar offsets - one in four frames in state 2, one
/// a frame in state 3, two a frame in state 4 - and state 5 fades the last of
/// the flash before releasing the work block. `Gp_SpawnPadLerp` rumbles at each
/// state change, hardest on the widest row.
void func_apobiosis_8012EF4C(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    s32               i;
    s32               n;
    s32               pan;
    u8                rgb[3];

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0:
                D_apobiosis_80130BA0 = arg0;
                rot                  = (GfxRotationWords*)&coord->coord;
                coord->parent        = mem->parent;
                rot->m00M01          = ONE;
                rot->m02M10          = 0;
                rot->m11M12          = ONE;
                rot->m20M21          = 0;
                rot->m22             = ONE;
                coord->coord.t[0]    = 0;
                coord->coord.t[1]    = 0;
                coord->coord.t[2]    = 0;
                coord->composeStamp  = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                pan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(D_apobiosis_80130B74[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                         (s8)worldCoordGetOriginAudioDepth(coord));
                arg0->state = 1;
                mem->index  = Gp_StateC08.attachId % 10 - 1;
                mem->scale  = 0x200;
                mem->period = 0x80;
                mem->step   = 0xF0;
                for (i = 0; i < D_apobiosis_80130B5C[mem->index].stripCount * 2; i++) {
                    gRandomLcgState         = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_apobiosis_80130B80[i] = (i << 10) + ((gRandomLcgState >> 16) & 0x3FF);
                }
                Gp_SpawnPadLerp(0xA, 0xFF, 8);
                /* fallthrough */
            case 1:
                actorRenderComposeCoord(coord);
                if (mem->age == 4) {
                    Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
                }
                func_apobiosis_8012F808(mem->step);
                rgb[0] = rgb[1]    = mem->step >> 2;
                rgb[2]             = mem->step >> 1;
                coord->workm.t[1] -= 0x400;
                mem->scale         = mem->scale + D_apobiosis_80130B5C[mem->index].radiusStep;
                func_apobiosis_8013017C(
                    &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[1], mem->age,
                    D_apobiosis_80130B5C[mem->index].playerSpriteScale, 0);
                glowDrawHalo(coord, mem->scale, 0x80, rgb);
                if (mem->age & 1) {
                    glowDrawHalo(coord, 0x80, mem->scale, rgb);
                }
                for (i = 0; i < D_apobiosis_80130B5C[mem->index].stripCount; i++) {
                    gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_apobiosis_80130B80[i] -= ((gRandomLcgState >> 16) & 0xFF) - 0x80;
                    n                        = i + D_apobiosis_80130B5C[mem->index].radiusStep;
                    gRandomLcgState          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    D_apobiosis_80130B80[n] -= ((gRandomLcgState >> 16) & 0xFF) - 0x80;
                    mem->pos.vx              = mem->scale * rsin(D_apobiosis_80130B80[i]) >> 12;
                    mem->pos.vy              = mem->scale * rcos(D_apobiosis_80130B80[i]) >> 12;
                    mem->pos.vz =
                        mem->pos.vx *
                            rcos(D_apobiosis_80130B80
                                     [i + D_apobiosis_80130B5C[mem->index].radiusStep]) >>
                        12;
                    func_apobiosis_80130630(coord, &mem->pos, mem->age,
                                            D_apobiosis_80130B5C[mem->index].stripScale);
                }
                coord->workm.t[1] += 0x400;
                if (mem->step >= 0x19) {
                    mem->step = mem->step - 0x18;
                    return;
                }
                arg0->state = 2;
                Gp_SpawnPadLerp(0x14, 0xFF, 8);
                return;
            case 2:
                actorRenderComposeCoord(coord);
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 0x41) {
                    mem->step = mem->step - 0x10;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                if (((gRandomLcgState >> 16) & 3) == 0) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->scale      = (gRandomLcgState >> 16) & 0x3FF;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
                    mem->move.vx    = mem->scale * rsin(mem->angle) >> 12;
                    mem->move.vz    = mem->scale * rcos(mem->angle) >> 12;
                    Gp_SpawnEff(EFFECT_APOBIOSIS_SHARD, coord, 0, &mem->move);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->step       = ((gRandomLcgState >> 16) & 0x7F) + 0x60;
                }
                if (mem->age == 0x14) {
                    mem->step   = 0xF0;
                    arg0->state = 3;
                }
                return;
            case 3:
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 0x21) {
                    mem->step = mem->step - 0xC;
                }
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->scale      = (gRandomLcgState >> 16) & 0x7FF;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
                mem->move.vx    = mem->scale * rsin(mem->angle) >> 12;
                mem->move.vz    = mem->scale * rcos(mem->angle) >> 12;
                Gp_SpawnEff(EFFECT_APOBIOSIS_SHARD, coord, 0, &mem->move);
                if (mem->age == 0x1E) {
                    if (mem->index <= 0) {
                        arg0->state = 5;
                        Gp_SpawnPadLerp(mem->index * 8 + 0x12, 0xFF, 8);
                    } else {
                        mem->step   = 0xF0;
                        arg0->state = 4;
                        Gp_SpawnPadLerp(0x22, 0xFF, 8);
                    }
                }
                return;
            case 4:
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 9) {
                    mem->step = mem->step - 8;
                }
                for (i = 0; i < 2; i++) {
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
                    mem->move.vx    = mem->scale * rsin(mem->angle) >> 12;
                    mem->move.vz    = mem->scale * rcos(mem->angle) >> 12;
                    Gp_SpawnEff(EFFECT_APOBIOSIS_SHARD, coord, 0, &mem->move);
                }
                if (mem->age == 0x28) {
                    arg0->state = 5;
                }
                return;
            case 5:
                func_apobiosis_8012F808(mem->step);
                if (mem->step >= 9) {
                    mem->step = mem->step - 8;
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(mem, arg0);
}

/// Flashes a screen-filling `POLY_F4` over the whole 320x240 frame, offset by
/// `gDisplayState.vramYOffset` so it tracks the active draw buffer. `bright`
/// is the flash level: normally the quad is blue-tinted (red and green
/// halved), but on stage `Gp_StateC08.attachId % 10 == 3` one draw in four
/// comes out yellow instead (blue halved). The prim is linked at a fixed
/// `otz` of 0x30, in front of the scene.
static void func_apobiosis_8012F808(s16 bright)
{
    POLY_F4* prim;

    prim           = gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setPolyF4(prim);
    if ((u16)(Gp_StateC08.attachId % 10U) - 1 == 2 && (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) == 0) {
        setRGB0(prim, bright, bright, bright >> 1);
    } else {
        setRGB0(prim, bright >> 1, bright >> 1, bright);
    }
    setXY4(prim, -0xA0, -0x78 - gDisplayState.vramYOffset, 0xA0,
           -0x78 - gDisplayState.vramYOffset, -0xA0, 0x78 - gDisplayState.vramYOffset,
           0xA0, 0x78 - gDisplayState.vramYOffset);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET((((u32)(0x30 << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
    gpuSetPrimitiveBlendMode(prim, GPU_BLEND_ADD, 0x30);
}

#define GLOW_DRAW_HALO_PULL 0x40
#include "../../shared/glow_draw_halo.inc.c"

/// One shard of the apobiosis burst. Every frame it ticks the shard's life
/// counter `EffectWork.age` and bails out - handing the work block back -
/// once the player is dying (`Gp_StateC08.effectPhase`), parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`)
/// or the shard has outlived its state. State 0 reparents the shard onto the
/// cast task and splits on `spawnArg1`: a non-zero arg pins the shard to the
/// cast's coordinate at the origin (state 1), a zero arg gives it a random
/// drift `move` and lets it fly (state 2). Either way the tail
/// seeds the shard's `pos` offset, its radius `angle` and
/// the intensity `step` that picks a `D_apobiosis_80130B5C` row. Both live
/// states redraw the shard every other frame, at twice the row's radius while
/// pinned and at the plain radius once free.
void func_apobiosis_8012FE10(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if ((Gp_StateC08.effectPhase != ATTACHMENT_EFFECT_HELD) && (gRoomEffectState->peEffectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
        mem->age = mem->age + 1;
        switch (arg0->state) {
            case 0:
                taskReparent(D_apobiosis_80130BA0, arg0);
                if (arg0->spawnArg1.value != 0) {
                    coord->parent       = mem->parent;
                    coord->coord.t[0]   = 0;
                    coord->coord.t[1]   = 0;
                    coord->coord.t[2]   = 0;
                    coord->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(coord);
                    arg0->state = 1;
                } else {
                    mem->move.vy    = 0;
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vx    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    mem->move.vz    = 0x40 - ((gRandomLcgState >> 16) & 0x7F);
                    arg0->state     = 2;
                }
                mem->pos.vy     = -0x1000;
                mem->scale      = 0x80;
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vx     = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->pos.vz     = 0x800 - ((gRandomLcgState >> 16) & 0xFFF);
                gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
                mem->step       = Gp_StateC08.attachId % 10 - 1;
                return;
            case 1:
                actorRenderComposeCoord(coord);
                if (mem->age & 1) {
                    mem->index = mem->index + 1;
                    func_apobiosis_8013017C(coord, mem->index,
                                            D_apobiosis_80130B5C[mem->step].stripScale * 2,
                                            mem->angle);
                    func_apobiosis_80130630(coord, &mem->pos, mem->index,
                                            D_apobiosis_80130B5C[mem->step].stripScale * 2);
                }
                if (mem->age < 0x19) {
                    return;
                }
                break;
            case 2:
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(coord);
                if (mem->age & 1) {
                    mem->index = mem->index + 1;
                    func_apobiosis_8013017C(coord, mem->index,
                                            D_apobiosis_80130B5C[mem->step].stripScale,
                                            mem->angle);
                    func_apobiosis_80130630(coord, &mem->pos, mem->index,
                                            D_apobiosis_80130B5C[mem->step].stripScale);
                }
                if (mem->age < 0x11) {
                    return;
                }
                break;
            default:
                return;
        }
    }
    effectKillTask(mem, arg0);
}

/// One textured shard of the apobiosis burst. Projects `arg0`'s world
/// position through `GsWSMATRIX` with a single `RTPS` and, when the flag comes
/// back non-negative, queues one semi-transparent `POLY_FT4` at the projected
/// point. `arg1 % 6` picks one of the six 0x28-wide frames on tpage 0x2A - the
/// caller passes the shard's life counter, so the sprite animates - and `arg2`
/// sizes it: the corners sit `arg2 * 0x27 / otz` from the centre along `arg3`
/// and `arg3 + 0x400`, so the shard shrinks with depth and spins with `arg3`.
/// The CLUT is 0x4293 except on the widest combo row
/// (`Gp_StateC08.attachId % 10 - 1 == 2`), where one draw in four rolls the
/// brighter 0x42C9 palette. Same shape as Combustion's and Pyrokinesis's flame
/// quad (`func_combustion_8012FB14`), which uses a fixed CLUT and 0x20-wide
/// frames.
static void func_apobiosis_8013017C(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* head;
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s16                 frame;
    s32                 u0;
    s32                 u1;
    s32                 ang2;

    head                       = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    (head - 1)->worldPoint.vx  = arg0->workm.t[0];
    SCRATCH_STACK_CURSOR(void) = head - 1;
    block                      = SCRATCH_STACK_CURSOR(EffectShapeScratch);
    block->worldPoint.vy       = arg0->workm.t[1];
    block->worldPoint.vz       = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&(head - 1)->screenX);
    gte_stflg(&(head - 1)->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&(head - 1)->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        prim->tpage = 0x2A;
        if ((u16)(Gp_StateC08.attachId % 10) - 1 == 2) {
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            if (((gRandomLcgState >> 16) & 3) == 0) {
                prim->clut = 0x42C9;
            } else {
                prim->clut = 0x4293;
            }
        } else {
            prim->clut = 0x4293;
        }
        frame = arg1 % 6;
        u0    = frame * 0x28;
        u1    = u0 + 0x27;
        setUV4(prim, u0, 0x38, u1, 0x38, u0, 0x5F, u1, 0x5F);
        block->extent.corner.x = (((arg2 * 0x27) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 0x27) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + (u16)block->extent.corner.x;
        prim->x3               = block->screenX - (u16)block->extent.corner.x;
        prim->y0               = block->screenY - (u16)block->extent.corner.y;
        prim->y3               = block->screenY + (u16)block->extent.corner.y;
        ang2                   = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 0x27) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 0x27) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + (u16)block->extent.corner.x;
        prim->x2               = block->screenX - (u16)block->extent.corner.x;
        prim->y1               = block->screenY - (u16)block->extent.corner.y;
        prim->y2               = block->screenY + (u16)block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws one apobiosis burst shard as a semi-transparent raw-tex `POLY_FT4`
/// (tpage 0x28). The effect coordinate's world position and that position plus
/// `arg1` are each projected through `GsWSMATRIX` with one `RTPS`; the quad is
/// laid along the line joining the two projected points, `ratan2` of their
/// screen delta giving the spin applied at that angle and at `+ 0x400`. `arg2`
/// selects the 128-texel UV tile: u = `(arg2 & 1) * 128`, v =
/// `((arg2 & 3) >> 1) * 24 - 0x30`. `arg3` is a signed half-extent, so the
/// on-screen half-width is `arg3 * 23 / depth`. Clut is 0x4287, or 0x42C8 on
/// one in four LCG rolls when the combo row is 2. Nothing is drawn if either
/// projection sets a negative `gte_stflg`.
static void func_apobiosis_80130630(GfxCoord* arg0, SVECTOR* arg1, s16 arg2, s16 arg3)
{
    EffectStripScratch* block;
    POLY_FT4*           prim;
    s32                 u0;
    s32                 u1;
    s32                 va;
    s32                 vb;
    s16                 ang;

    block              = SCRATCH_STACK_RESERVE_BLOCK(EffectStripScratch);
    block->worldEnd.vx = block->worldStart.vx = arg0->workm.t[0];
    block->worldEnd.vy = block->worldStart.vy = arg0->workm.t[1];
    block->worldEnd.vz = block->worldStart.vz = arg0->workm.t[2];
    block->worldEnd.vx                       += arg1->vx;
    block->worldEnd.vy                       += arg1->vy;
    block->worldEnd.vz                       += arg1->vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldStart);
    gte_rtps();
    gte_stsxy(&block->screenStart);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        gte_ldv0(&block->worldEnd);
        gte_rtps();
        gte_stsxy(&block->screenEnd);
        gte_stflg(&block->projectionFlags);
        if (block->projectionFlags >= 0) {
            prim           = gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            prim->tpage = 0x28;
            if ((u16)(Gp_StateC08.attachId % 10U) - 1 == 2 &&
                (((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 3) == 0) {
                prim->clut = 0x42C8;
            } else {
                prim->clut = 0x4287;
            }
            u0 = (arg2 & 1) << 7;
            u1 = u0 + 0x7F;
            va = ((arg2 & 3) >> 1) * 24 - 0x30;
            vb = ((arg2 & 3) >> 1) * 24 - 0x19;
            setUV4(prim, u0, va, u1, va, u0, vb, u1, vb);
            ang                  = ratan2(block->screenEnd.vy - block->screenStart.vy, block->screenEnd.vx - block->screenStart.vx);
            block->cornerOffsetX = (((arg3 * 0x17) / block->depth) * rsin(ang)) >> 12;
            block->cornerOffsetY = (((arg3 * 0x17) / block->depth) * rcos(ang)) >> 12;
            prim->x0             = block->screenStart.vx + block->cornerOffsetX;
            prim->x3             = block->screenEnd.vx - block->cornerOffsetX;
            prim->y0             = block->screenStart.vy - block->cornerOffsetY;
            prim->y3             = block->screenEnd.vy + block->cornerOffsetY;
            block->cornerOffsetX = (((arg3 * 0x17) / block->depth) * rsin(ang + 0x400)) >> 12;
            block->cornerOffsetY = (((arg3 * 0x17) / block->depth) * rcos(ang + 0x400)) >> 12;
            prim->x1             = block->screenEnd.vx + block->cornerOffsetX;
            prim->x2             = block->screenStart.vx - block->cornerOffsetX;
            prim->y1             = block->screenEnd.vy - block->cornerOffsetY;
            prim->y2             = block->screenStart.vy + block->cornerOffsetY;
            addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                    prim);
        }
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectStripScratch);
}
