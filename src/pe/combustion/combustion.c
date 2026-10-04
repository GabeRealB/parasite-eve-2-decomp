#include "pe/combustion.h"

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
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/pyro_flame.h"
#include "../../shared/sprite_quad.h"

/// Reach and duration of the combustion flame lines at one Parasite Energy
/// level.
///
/// The cast runs two flame emitters, each laying one flame per frame along a
/// line that leaves the caster to one side of its facing and descends as it
/// goes. An emitter and every flame it lays select a row by the level digit of
/// the spell being cast (`AttachmentState::attachId % 10 - 1`), so a higher
/// level lays more flames along a longer, shallower line and burns for longer.
///
/// The steps are distances along the emitter coordinate's own axes. The frame
/// counts are compared with `EffectWork::age`, which is 1 on an emitter's
/// first frame.
typedef struct {
    s16 flameDropStep;     // Added each frame to the Y of the offset the next flame is laid at; positive is down
    s16 flameReachStep;    // Added each frame to the Z of that offset, which starts 0x200 out
    s16 narrowFlameFrames; // Frames an emitter lays narrow flames for. The flame of the frame after is drawn wide and ends the line, and the spell's stats are applied on that frame
    s16 emitterFrames;     // Frames an emitter lives; also the length of the controller vibration that fades out under it
} _CombustionLevelTuning;
STATIC_ASSERT_SIZEOF(_CombustionLevelTuning, 0x8);

static void func_combustion_8012F5EC(GfxCoord* arg0, s16 arg1, s16 arg2);
static void func_combustion_8012FF0C(GfxCoord* arg0, s32 arg1, s16 arg2);
static void func_combustion_801305F8(GfxCoord* arg0, s16 arg1, s16 arg2);

/// Per-level tuning for the combustion flame, one row per PE level 1-3,
/// weakest first.
static _CombustionLevelTuning D_combustion_80130980[] = {
    { 0x0060, 0x0120, 0x0007, 0x0015 },
    { 0x0055, 0x0187, 0x0008, 0x0017 },
    { 0x004C, 0x01F3, 0x0009, 0x0019 },
};

/// The `SndEvt_EnqueueType6` id for each `D_combustion_80130980` row.
static s32 D_combustion_80130998[] = { 0xE00C0002, 0xE00F0002, 0xE0120002 };

/// The effect coordinate's world Y at ignition, saved by
/// `func_combustion_8012EF34` before it re-bases the coordinate on the player.
static s32 D_combustion_801309A4 = 0;

/// Burns the player: parents an effect coordinate to the player model, plays
/// the ignition sound and fades the screen, then spawns a flame every frame
/// while drifting the flame overlay by the `D_combustion_80130980` row for the
/// current intensity. State 1 spawns, state 2 (past `narrowFlameFrames`) only
/// unwinds the yaw the ignition applied, and either state ends as soon as the
/// player is dying (`Gp_StateC08.effectPhase`), parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`) or the row's
/// `emitterFrames` tick is reached.
void func_combustion_8012EF34(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    EffectWork*       spawned;
    s32               pan;
    u8                rgb[3];

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            if (arg0->spawnArg1.value == 0) {
                arg0->spawnArg1.value = 1;
            }
            D_combustion_801309A4 = coord->workm.t[1];
            rot                   = (GfxRotationWords*)&coord->coord;
            coord->parent         = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            rot->m00M01           = ONE;
            rot->m11M12           = ONE;
            rot->m22              = ONE;
            rot->m02M10           = 0;
            rot->m20M21           = 0;
            coord->coord.t[0]     = 0;
            coord->coord.t[1]     = -0x400;
            coord->coord.t[2]     = 0;
            gfxRotMatrixY(&coord->coord, arg0->spawnArg1.value << 9, 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            mem->move.vz = 0x200;
            pan          = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(D_combustion_80130998[(u16)(Gp_StateC08.attachId % 10) - 1], pan,
                                (s8)worldCoordGetOriginAudioDepth(coord));
            rgb[0] = 0xFF;
            rgb[1] = 0x7F;
            rgb[2] = 0x3F;
            Gp_DrawFadeQuad(rgb, 1);
            arg0->state = 1;
            mem->index  = Gp_StateC08.attachId % 10 - 1;
            Gp_SpawnPadLerp(D_combustion_80130980[mem->index].emitterFrames, 0xFF, 8);
            /* fallthrough */
        case 1:
            actorRenderComposeCoord(coord);
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                goto release;
            }
            mem->move.vy = mem->move.vy + D_combustion_80130980[mem->index].flameDropStep;
            mem->move.vz = mem->move.vz + D_combustion_80130980[mem->index].flameReachStep;
            spawned      = Gp_SpawnEff((EFFECT_COMBUSTION_FLAME | EFFECT_SPAWN_UNLIMITED), coord, (s32)(mem->age), &mem->move);
            if (spawned != NULL) {
                taskReparent(arg0, spawned->task);
            }
            if (D_combustion_80130980[mem->index].narrowFlameFrames < mem->age) {
                Gp_StateC08.flags |= ATTACHMENT_FLAG_APPLY_STATS;
                arg0->state        = 2;
                return;
            }
            return;
        case 2:
            actorRenderComposeCoord(coord);
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) ||
                (mem->age > D_combustion_80130980[mem->index].emitterFrames)) {
            release:
                effectKillTask(mem, arg0);
                return;
            }
            gfxRotMatrixY(&coord->coord, -(arg0->spawnArg1.value * 80), 0);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            break;
    }
}

/// One flame of the combustion burn. State 0 re-bases the effect coordinate on
/// the `EffectWork.parent` parent with an identity rotation and the work
/// block's `pos` offset, seeds the phase `age` from
/// `gRandomLcgState`, the radius `scale` from `spawnArg1` and the intensity
/// `index` from `Gp_StateC08.attachId % 10 - 1`, then splits: `spawnArg1`
/// past the `D_combustion_80130980` row's `narrowFlameFrames` runs the wide
/// state 2, anything smaller the narrow state 1. Both states redraw every frame -
/// `index < 2` picks the small draw helper, otherwise the large one - and
/// one frame in four spawn a trailing ember that adopts this task as its
/// parent. Either state releases the effect once the player is dying
/// (`Gp_StateC08.effectPhase`), parasite-energy effects are cancelled
/// (`gRoomEffectState->peEffectControl`) or the flame has lived
/// 0x21 frames.
void func_combustion_8012F2BC(Task* arg0)
{
    EffectWork*       mem;
    GfxCoord*         coord;
    GfxRotationWords* rot;
    EffectWork*       spawned;
    s32               rng;
    s32               spawnRng1;
    s32               spawnRng1b;
    s32               spawnRng2;
    s32               spawnRng2b;
    s32               last;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            rot           = (GfxRotationWords*)&coord->coord;
            coord->parent = mem->parent;
            rot->m00M01   = ONE;
            rot->m02M10   = 0;
            rot->m11M12   = ONE;
            rot->m20M21   = 0;
            rot->m22      = ONE;

            coord->coord.t[0]   = mem->pos.vx;
            coord->coord.t[1]   = mem->pos.vy;
            coord->coord.t[2]   = mem->pos.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);

            rng             = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->age        = ((u32)rng >> 16) & 0xF;
            mem->scale      = arg0->spawnArg1.value * 32 + 512;
            mem->index      = Gp_StateC08.attachId % 10 - 1;
            last            = D_combustion_80130980[mem->index].narrowFlameFrames;
            gRandomLcgState = rng;
            if (last < arg0->spawnArg1.value) {
                arg0->state = 2;
                return;
            }
            arg0->state = 1;
            return;
        case 1:
            actorRenderComposeCoord(coord);
            if (mem->index < 2) {
                func_combustion_8012F5EC(coord, mem->age, mem->scale);
            } else {
                func_combustion_801305F8(coord, mem->age, mem->scale);
            }
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (mem->age >= 0x21)) {
                effectKillTask(mem, arg0);
                return;
            }
            spawnRng1       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = spawnRng1;
            if ((((u32)spawnRng1 >> 16) & 3) == 0) {
                spawnRng1b      = spawnRng1 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = spawnRng1b;
                spawned         = Gp_SpawnEff(EFFECT_COMBUSTION_EMBER, coord, ((u32)spawnRng1b >> 16) & 1, 0);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            return;
        case 2:
            actorRenderComposeCoord(coord);
            if (mem->index < 2) {
                spriteQuadDrawFlicker(coord, mem->age, mem->scale * 3 / 2, 0);
            } else {
                spriteQuadDrawFlicker(coord, mem->age, mem->scale * 4, 0);
            }
            if ((Gp_StateC08.effectPhase == ATTACHMENT_EFFECT_HELD) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) || (mem->age >= 0x21)) {
                effectKillTask(mem, arg0);
                return;
            }
            spawnRng2       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = spawnRng2;
            if ((((u32)spawnRng2 >> 16) & 3) == 0) {
                spawnRng2b      = spawnRng2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = spawnRng2b;
                spawned         = Gp_SpawnEff(EFFECT_COMBUSTION_EMBER, coord, ((u32)spawnRng2b >> 16) & 1, 0);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
            }
            return;
    }
}

/// Links one frame of the small combustion flame at `arg0`'s world position.
/// The position is projected through `GsWSMATRIX` by a single `RTPS` and the
/// quad is dropped when that sets a negative `gte_stflg`. `arg1 % 6` picks one
/// of the six 0x20-wide texture frames on tpage 0x29 (CLUT 0x4282), and `arg2`
/// sizes it: the corners sit `arg2 * 31 / depth` from the projected centre.
/// Same 0x18-byte scratch and axis-aligned quad as `func_combustion_8012FF0C`.
static void func_combustion_8012F5EC(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    s32                  u0;
    s32                  u1;
    s16                  x;
    s16                  y;
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
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage         = 0x29;
        prim->clut          = 0x4282;
        prim->v0            = 0x98;
        prim->v1            = 0x98;
        prim->v2            = 0xB7;
        prim->v3            = 0xB7;
        u0                  = (s16)(arg1 % 6) * 0x20;
        u1                  = u0 + 0x1F;
        prim->u1            = u1;
        prim->u3            = u1;
        prim->u0            = u0;
        prim->u2            = u0;
        block->screenExtent = (arg2 * 0x1F) / block->depth;
        x                   = block->screenX - (u16)block->screenExtent;
        prim->x2            = x;
        prim->x0            = x;
        x                   = block->screenX + (u16)block->screenExtent;
        prim->x3            = x;
        prim->x1            = x;
        y                   = block->screenY - (u16)block->screenExtent;
        prim->y1            = y;
        prim->y0            = y;
        y                   = block->screenY + (u16)block->screenExtent;
        prim->y3            = y;
        prim->y2            = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

/// One trailing ember shed by a `func_combustion_8012F2BC` flame. State 0 rolls
/// the kind from `Gp_StateC08.attachId % 10 - 1` into `step`, two
/// `gRandomLcgState` draws into the spin `scale` and the per-frame rise
/// `move.vy` (`-(rand & 0xFF) - kind * 64`, so bigger embers climb faster),
/// sizes the sprite as `kind * 0x100 + 0x300` in `angle`, and enters
/// `spawnArg1 + 1` - or one state later on a coin flip when `kind >= 2`. Every
/// later state lifts the coordinate by `move.vy` and redraws: state 1 steps
/// `index` every other frame and draws the `pyroFlameDrawSprite` flame
/// on the odd frames, state 2 draws `func_combustion_8012FF0C` and state 3 the
/// small `func_combustion_8012F5EC`, each releasing the ember after eight (six
/// for state 3) frames.
void func_combustion_8012F888(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         rng;
    s32         rng2;
    s32         y;
    s16         step;
    s16         kind;
    s16         frame;
    s32         state;
    s32         tmp;
    s32         hi;
    s32         tmp2;

    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    mem->age = mem->age + 1;
    state    = arg0->state;
    switch (state) {
        case 0:
            kind       = (Gp_StateC08.attachId % 10U) - 1;
            rng        = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            rng2       = rng * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale = ((u32)rng2 >> 16) & 0xFFF;
            hi         = ((u32)rng >> 16) & 0xFF;
            mem->step  = kind;
            /* The global store between the `field_2A` store and its reload keeps
             * GCC from forwarding `kind` into the `lh`. */
            gRandomLcgState = rng;
            tmp             = mem->step;
            mem->move.vy    = -hi - (tmp << 6);
            gRandomLcgState = rng2;
            arg0->state     = arg0->spawnArg1.value + 1;
            tmp2            = mem->step;
            mem->angle      = (tmp2 << 8) + 0x300;
            if (mem->step >= 2) {
                gRandomLcgState = rng2 * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                arg0->state    += (gRandomLcgState >> 16) & 1;
            }
            /* fallthrough */
        case 1:
            step                = mem->move.vy;
            y                   = coord->coord.t[1] + step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            actorRenderComposeCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            frame = mem->index;
            if (frame < 8) {
                if (mem->age & 1) {
                    pyroFlameDrawSprite(coord, frame, mem->angle, mem->scale);
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
            frame      = mem->index + 1;
            mem->index = frame;
            if (frame < 8) {
                func_combustion_8012FF0C(coord, frame, mem->angle);
                return;
            }
            effectKillTask(mem, arg0);
            return;
        case 3:
            step                = mem->move.vy;
            y                   = coord->coord.t[1] + step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            actorRenderComposeCoord(coord);
            frame      = mem->index + 1;
            mem->index = frame;
            if (frame < 6) {
                func_combustion_8012F5EC(coord, frame, mem->angle);
                return;
            }
            effectKillTask(mem, arg0);
            return;
    }
}

#include "../../shared/pyro_flame_draw_sprite.inc.c"

/// Links one frame of the combustion flame at `arg0`'s world position. The
/// position is projected through `GsWSMATRIX` by a single `RTPS` and the quad
/// is dropped when that sets a negative `gte_stflg`. `arg1` picks one of the
/// eight 0x18-wide texture frames on tpage 0x28 (CLUT 0x430D), and `arg2`
/// sizes it: the corners sit `arg2 * 23 / depth` from the projected centre, so
/// the sprite shrinks with depth. Same 0x18-byte scratch and axis-aligned
/// quad as gameplay `Gp_EffSprTask8D`.
static void func_combustion_8012FF0C(GfxCoord* arg0, s32 arg1, s16 arg2)
{
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    s32                  u0;
    s16                  x;
    s16                  y;
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
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x28;
        prim->clut  = 0x430D;
        u0          = (arg1 & 7) * 0x18;
        setUV4(prim, u0, 0xA0, u0 + 0x17, 0xA0, u0, 0xB7, u0 + 0x17, 0xB7);
        block->screenExtent = (arg2 * 0x17) / block->depth;
        x                   = block->screenX - (u16)block->screenExtent;
        prim->x2            = x;
        prim->x0            = x;
        x                   = block->screenX + (u16)block->screenExtent;
        prim->x3            = x;
        prim->x1            = x;
        y                   = block->screenY - (u16)block->screenExtent;
        prim->y1            = y;
        prim->y0            = y;
        y                   = block->screenY + (u16)block->screenExtent;
        prim->y3            = y;
        prim->y2            = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

#define SPRITE_QUAD_SCALE 55
#define SPRITE_QUAD_ODD_LOOK(p)   \
    setRGB0(p, 0xC0, 0x70, 0x40); \
    SPRITE_QUAD_CORE_CELL(p);     \
    setSemiTrans(p, 1)
#define SPRITE_QUAD_EVEN_LOOK(p) \
    setcode(p, 0x2F);            \
    SPRITE_QUAD_RIM_CELL(p)
#include "../../shared/sprite_quad_draw_flicker.inc.c"

/// Links one frame of the large combustion flame at `arg0`'s world position,
/// the same way `func_combustion_8012F5EC` does the small one: the position is
/// projected through `GsWSMATRIX` by a single `RTPS` and the quad is dropped
/// when that sets a negative `gte_stflg`. `arg1 % 12` picks a cell of the
/// 6x2 sheet of 0x28-pixel frames on tpage 0x2A, each with its own CLUT
/// (`0x4300` plus the cell index), and `arg2` sizes it: the corners sit
/// `arg2 * 39 / depth` from the projected centre, so the sprite shrinks with
/// depth.
static void func_combustion_801305F8(GfxCoord* arg0, s16 arg1, s16 arg2)
{
    u8*                  head;
    EffectCentreScratch* block;
    POLY_FT4*            prim;
    SVECTOR*             vec;
    s32                  frame;
    u32                  cell;
    u16                  col;
    u16                  row;
    s32                  u0;
    s32                  u1;
    s32                  v0;
    s32                  v2;
    s16                  x;
    s16                  y;
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
    frame = arg1;
    frame = frame % 12;
    gte_stsxy(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->screenX);
    gte_stflg(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&((EffectCentreScratch*)(head - sizeof(EffectCentreScratch)))->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2F);
        prim->tpage = 0x2A;
        cell        = (u16)frame;
        prim->clut  = (cell & 0x3F) | 0x4300;
        col         = cell % 6;
        row         = cell / 6;
        u0          = col * 0x28;
        u1          = u0 + 0x27;
        v0          = row * 0x28 - 0x78;
        v2          = row * 0x28 - 0x51;
        setUV4(prim, u0, v0, u1, v0, u0, v2, u1, v2);
        block->screenExtent = (arg2 * 0x27) / block->depth;
        x                   = block->screenX - (u16)block->screenExtent;
        prim->x2            = x;
        prim->x0            = x;
        x                   = block->screenX + (u16)block->screenExtent;
        prim->x3            = x;
        prim->x1            = x;
        y                   = block->screenY - (u16)block->screenExtent;
        prim->y1            = y;
        prim->y0            = y;
        y                   = block->screenY + (u16)block->screenExtent;
        prim->y3            = y;
        prim->y2            = y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectCentreScratch);
}

void func_combustion_801308E0(Task* arg0)
{
    GfxCoord* coord;

    if (arg0->state != 0) {
        effectKillTask(arg0->spawnArg2.pointer, arg0);
        return;
    }
    coord = arg0->extra.coordBody->coord;
    actorRenderComposeCoord(coord);
    Gp_SpawnEff((EFFECT_COMBUSTION_FLAME_EMITTER | EFFECT_SPAWN_UNLIMITED), coord, 1, 0);
    Gp_SpawnEff((EFFECT_COMBUSTION_FLAME_EMITTER | EFFECT_SPAWN_UNLIMITED), coord, -1, 0);
    arg0->state = arg0->state + 1;
}
