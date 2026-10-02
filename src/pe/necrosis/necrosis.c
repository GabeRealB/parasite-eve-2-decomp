#include "pe/necrosis.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "gameplay/display.h"
#include "gameplay/actor.h"
#include "gameplay/actor_render.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/effects.h"
#include "gameplay/pad_script.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
/// Signed texture-frame selector; the caller reduces the effect age modulo six.
#define SPRITE_QUAD_FRAME_T s16
#include "../../shared/sprite_quad.h"

/// One 4-byte row of `D_necrosis_801306BC`, indexed by `EffectWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the `Gp_SpawnEff` draw
/// parameter (plus `field_22 * 0x60` each frame) and is copied into the
/// first `WorldCollisionBody.radius`. `field_2` is the last `EffectWork.age` tick
/// of the spawn loop; state 2 waits an extra 0x10 ticks past it. `field_2 +
/// 0xC` is also the pad-rumble duration at ignition.
typedef struct NecrosisStep {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ s16 field_2;
} NecrosisStep;
STATIC_ASSERT_SIZEOF(NecrosisStep, 4);

/// Collision pair allocated by `func_necrosis_8012EF34` (`memCalloc(0x58)`)
/// and stored in `Task::work`. `obj` is linked on list 1, `obj2` on list 7;
/// both point `context.contacts` at the one-element `rec` table (terminator `field_0
/// = 2`).
typedef struct NecrosisWork {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionBody    obj2;
    /* 0x40 */ WorldCollisionContact rec;
} NecrosisWork;
STATIC_ASSERT_SIZEOF(NecrosisWork, 0x58);

/// Per-level tuning for the necrosis burst: rows are PE levels 1-3, selected
/// by `index`. `field_0` is the `Gp_SpawnEff` draw parameter; `field_2` is
/// the last spawn-loop tick, and `field_2 + 0xC` the pad-rumble duration.
static NecrosisStep D_necrosis_801306BC[] = {
    { 0x03C0, 0x000A },
    { 0x0480, 0x000F },
    { 0x0540, 0x0014 },
};

/// The `SndEvt_EnqueueType6` id for each `D_necrosis_801306BC` row.
static s32 D_necrosis_801306C8[] = { 0xE0150001, 0xE0180001, 0xE01B0001 };

static void func_necrosis_8012FE64(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);
static void func_necrosis_80130288(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3);

/// Runs one frame of the necrosis cast. State 0 copies the player rotation onto
/// the effect coordinate, rotates a (0, 0, 0x90) offset into that frame, and
/// links a `NecrosisWork` collision pair (list 1 + list 7) whose packed id is
/// the combo digits plus `0x28000`. State 1 GPF-scales that offset by 0x1100
/// each frame, walks the coordinate, and spawns `0x80060019`; a `0x100000` hit
/// on `obj2` zeros the offset and unlinks the list-7 object. State 2 waits
/// `field_2 + 0x10` ticks. Any state releases if the player is dying
/// (`Gp_StateC08.field_3` / `Gp_StateC08.field_3`) or parasite-energy effects are
/// cancelled (`gRoomEffectState->peEffectControl`).
void func_necrosis_8012EF34(Task* arg0)
{
    NecrosisWork*          work;
    EffectWork*            mem;
    GfxCoord*              coord;
    GfxCoord*              player;
    GfxRotationWords*      destinationRotation;
    GfxRotationWords*      sourceRotation;
    WorldCollisionContact* rec;
    EffectWork*            spawned;
    s32                    pan;
    u16                    old;
    s32                    tick;
    s16                    peEffectControl;

    work     = (NecrosisWork*)arg0->work;
    mem      = arg0->spawnArg2.pointer;
    coord    = arg0->extra.coordBody->coord;
    old      = mem->age;
    tick     = old + 1;
    mem->age = tick;
    switch (arg0->state) {
        case 0:
            if (Gp_StateC08.field_3 == -2) {
                goto release;
            }
            peEffectControl = gRoomEffectState->peEffectControl;
            if (peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto release;
            }
            if (peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                mem->age = old;
                return;
            }
            work = memCalloc(0x58, 0);
            if (work == NULL) {
                mem->age = 0;
                return;
            }
            player                      = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords;
            destinationRotation         = (GfxRotationWords*)&coord->coord;
            sourceRotation              = (GfxRotationWords*)&player->coord;
            destinationRotation->m00M01 = sourceRotation->m00M01;
            destinationRotation->m02M10 = sourceRotation->m02M10;
            destinationRotation->m11M12 = sourceRotation->m11M12;
            destinationRotation->m20M21 = sourceRotation->m20M21;
            destinationRotation->m22    = sourceRotation->m22;
            coord->composeStamp         = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            mem->move.vx = 0;
            mem->move.vy = 0;
            mem->move.vz = 0x90;
            gte_SetRotMatrix(&player->coord);
            gte_ldv0(&mem->move);
            gte_rtv0();
            gte_stsv(&mem->move);
            rec                        = &work->rec;
            mem->index                 = (Gp_StateC08.field_0 % 10) - 1;
            arg0->work                 = work;
            work->obj.coord            = coord;
            work->obj.context.contacts = rec;
            work->obj.key =
                ((u16)(Gp_StateC08.field_0 / 100) - 1) * 9 + ((u16)((u16)(Gp_StateC08.field_0 % 100) / 10) - 1) * 3 + (u16)(Gp_StateC08.field_0 % 10) + 0x28000;
            work->obj.radius = D_necrosis_801306BC[mem->index].field_0;
            work->obj.flags  = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(1, &work->obj);
            rec->flags                  = 2;
            work->obj2.coord            = coord;
            work->obj2.context.contacts = rec;
            work->obj2.key              = 0;
            work->obj2.radius           = 0x80;
            work->obj2.flags            = WORLD_COLLISION_BODY_SPHERE;
            work->obj.flags            |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            Gp_LinkObj(7, &work->obj2);
            work->obj2.flags = (work->obj2.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED)) | (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED);
            pan              = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(D_necrosis_801306C8[(u16)(Gp_StateC08.field_0 % 10) - 1], pan,
                                (s8)worldCoordGetOriginAudioDepth(coord));
            Gp_SpawnPadLerp((s16)((u16)D_necrosis_801306BC[mem->index].field_2 + 0xC), 0xFF, 8);
            arg0->state = 1;
            /* fallthrough */
        case 1:
            if (gRoomEffectState->peEffectControl == ROOM_EFFECT_CONTROL_RUNNING) {
                gte_lddp(0x1100);
                gte_ldsv(&mem->move);
                gte_gpf12();
                gte_stsv(&mem->move);
                coord->coord.t[0]  += mem->move.vx;
                coord->coord.t[1]  += mem->move.vy;
                coord->coord.t[2]  += mem->move.vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                spawned = Gp_SpawnEff((EFFECT_NECROSIS_TRAIL_PUFF | EFFECT_SPAWN_UNLIMITED), coord,
                                      (s16)D_necrosis_801306BC[mem->index].field_0 + (mem->age * 0x60),
                                      NULL);
                if (spawned != NULL) {
                    taskReparent(arg0, spawned->task);
                }
                work->obj.radius = work->obj.radius + 0x20;
            } else {
                mem->age = mem->age - 1;
            }
            if ((Gp_StateC08.field_3 == -2) || (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN)) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                goto release;
            }
            if (mem->age > D_necrosis_801306BC[mem->index].field_2) {
                Gp_UnlinkObj(&work->obj);
                Gp_UnlinkObj(&work->obj2);
                arg0->state = 2;
                return;
            }
            if (Gp_FindRec18(work->obj2.context.contacts, 0x100000) != 0) {
                mem->move.vx = 0;
                mem->move.vy = 0;
                mem->move.vz = 0;
                Gp_UnlinkObj(&work->obj2);
            }
            Gp_ClearRec18Occupied(&work->rec);
            return;
        case 2:
            if (Gp_StateC08.field_3 == -2) {
                goto release;
            }
            if (gRoomEffectState->peEffectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
                goto release;
            }
            tick = (s16)tick;
            if ((D_necrosis_801306BC[mem->index].field_2 + 0x10) < tick) {
            release:
                effectKillTask(mem, arg0);
            }
            break;
    }
}

void func_necrosis_8012F52C(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    EffectWork* spawned;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    mem->age = mem->age + 1;
    if (arg0->state == 0) {
        mem->scale      = arg0->spawnArg1.value & 0xFFF;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        mem->angle      = (gRandomLcgState >> 16) & 0xFFF;
        mem->period     = mem->scale - 0x100;
        mem->step       = mem->scale >> 4;
        arg0->state     = 1;
    }
    Gp_UpdateCoord(coord);
    spriteQuadDraw(coord, mem->age % 6, mem->scale, mem->angle);
    mem->scale = mem->scale - mem->step;
    if (mem->scale < mem->step) {
        effectKillTask(mem, arg0);
        return;
    }
    if (mem->age % 3 == 0) {
        spawned = Gp_SpawnEff(EFFECT_NECROSIS_MIST_PUFF, coord, (s32)(mem->period), 0);
        if (spawned != NULL) {
            taskReparent(arg0, spawned->task);
        }
    }
}

/// Trail-puff palette: VRAM X=240 words, Y=266 scanlines.
#define SPRITE_QUAD_CLUT getClut(240, 266)
/// Texel width and horizontal stride of each of the six frames selected by the caller.
#define SPRITE_QUAD_CELL_WIDTH 40
/// Inclusive top texel row of the necrosis strip, relative to its texture page.
///
/// Signed integer constant for the next drawer inclusion; see `sprite_quad.h`.
#define SPRITE_QUAD_TOP_V 0x38
#define SPRITE_QUAD_V1    0x5F
/// Perspective-sizing multiplier for the necrosis sprite.
///
/// Uses the cell's inclusive 39-texel UV span in `size * SPRITE_QUAD_SCALE / depth`.
#define SPRITE_QUAD_SCALE (SPRITE_QUAD_CELL_WIDTH - 1)
#include "../../shared/sprite_quad_draw.inc.c"

void func_necrosis_8012FAF8(Task* arg0)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s16         tick;
    s32         rng1;
    s32         rng2;
    s32         rng3;
    s32         temp_lo;
    s32         var_v1;
    u16         temp_v0;

    mem   = arg0->spawnArg2.pointer;
    coord = arg0->extra.coordBody->coord;
    if (gRoomEffectState->peEffectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        return;
    }

    mem->age = mem->age + 1;
    switch (arg0->state) {
        case 0:
            mem->age        = 0;
            temp_v0         = arg0->spawnArg1.value;
            mem->period     = temp_v0 & 0xFFF;
            rng1            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            mem->scale      = ((u32)rng1 >> 16) & 0xFFF;
            gRandomLcgState = rng1;
            mem->angle      = mem->period / 20;
            mem->move.vx    = (rsin(mem->scale) * mem->angle) >> 12;
            temp_lo         = rcos(mem->scale) * mem->angle;
            rng2            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng2;
            mem->move.vy    = temp_lo >> 12;
            mem->move.vz    = (rsin(((u32)rng2 >> 16) & 0xFFF) * mem->move.vx) >> 12;
            rng3            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = rng3;
            if ((s32)(((u32)rng3 >> 16) & 3) < ((u16)(Gp_StateC08.field_0 % 10U) - 1)) {
                mem->step = 0x1000;
            }
            if ((u16)(Gp_StateC08.field_0 % 10U) - 1 < 2) {
                arg0->state = 1;
                return;
            }
            var_v1 = 2;
            if (mem->step != 0) {
                var_v1 = 1;
            }
            arg0->state = var_v1;
            return;
        case 1:
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            tick       = mem->index + 1;
            mem->index = tick;
            if (tick < 8) {
                func_necrosis_8012FE64(coord, (s16)(tick | mem->step), mem->period,
                                       mem->scale);
                return;
            }
            effectKillTask(mem, arg0);
            return;
        case 2:
            coord->coord.t[0]  += mem->move.vx;
            coord->coord.t[1]  += mem->move.vy;
            coord->coord.t[2]  += mem->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            tick       = mem->index + 1;
            mem->index = tick;
            if (tick < 6) {
                func_necrosis_80130288(coord, (s16)(tick | mem->step), mem->period,
                                       mem->scale);
                return;
            }
            effectKillTask(mem, arg0);
            return;
    }
}

/// Draws one frame of the necrosis mist puff. `arg0`'s world position is
/// projected through `GsWSMATRIX` by a single `RTPS` and the quad is dropped
/// when that sets a negative `gte_stflg`. `arg1` is packed by the caller: the
/// low nibble picks one of the 0x20-wide texture cells on row 0x18..0x37, and
/// bit 0x1000 swaps the pale tpage/CLUT pair (0x2A / 0x428F) for the dark one
/// (0x4A / 0x42C2). `arg3` spins the quad and `arg2` sizes it: the corners sit
/// `arg2 * 31 / otz` from the projected centre along `arg3` and `arg3 + 0x400`,
/// so the puff shrinks with depth. Same shape as `spriteQuadDraw`.
static void func_necrosis_8012FE64(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s32                 u0;
    s32                 u1;
    s32                 ang2;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        if (arg1 & 0x1000) {
            prim->tpage = 0x2A;
            prim->clut  = 0x428F;
        } else {
            prim->tpage = 0x4A;
            prim->clut  = 0x42C2;
        }
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        u0 = (arg1 & 0xF) << 5;
        u1 = u0 + 0x1F;
        setUV4(prim, u0, 0x18, u1, 0x18, u0, 0x37, u1, 0x37);
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + block->extent.corner.x;
        prim->x3               = block->screenX - block->extent.corner.x;
        prim->y0               = block->screenY - block->extent.corner.y;
        prim->y3               = block->screenY + block->extent.corner.y;
        ang2                   = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 31) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 31) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + block->extent.corner.x;
        prim->x2               = block->screenX - block->extent.corner.x;
        prim->y1               = block->screenY - block->extent.corner.y;
        prim->y2               = block->screenY + block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}

/// Draws one frame of the necrosis spore cloud. Same shape as
/// `func_necrosis_8012FE64`: `arg0`'s world position is projected through
/// `GsWSMATRIX` by a single `RTPS` and the quad is dropped when that sets a
/// negative `gte_stflg`. `arg1` is packed by the caller: the low nibble picks
/// one of the 0x28-wide texture cells on row 0x50..0x77, and bit 0x1000 swaps
/// the dark tpage/CLUT pair (0x49 / 0x42C2) for the pale one (0x29 / 0x428F).
/// `arg3` spins the quad and `arg2` sizes it: the corners sit `arg2 * 39 / otz`
/// from the projected centre along `arg3` and `arg3 + 0x400`, so the cloud
/// shrinks with depth.
static void func_necrosis_80130288(GfxCoord* arg0, s16 arg1, s16 arg2, s16 arg3)
{
    EffectShapeScratch* block;
    POLY_FT4*           prim;
    s32                 u0;
    s32                 u1;
    s32                 ang2;

    block                = SCRATCH_STACK_RESERVE_BLOCK(EffectShapeScratch);
    block->worldPoint.vx = arg0->workm.t[0];
    block->worldPoint.vy = arg0->workm.t[1];
    block->worldPoint.vz = arg0->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->worldPoint);
    gte_rtps();
    gte_stsxy(&block->screenX);
    gte_stflg(&block->projectionFlags);
    if (block->projectionFlags >= 0) {
        gte_stszotz(&block->depth);
        block->depth++;
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setPolyFT4(prim);
        if (arg1 & 0x1000) {
            prim->tpage = 0x29;
            prim->clut  = 0x428F;
        } else {
            prim->tpage = 0x49;
            prim->clut  = 0x42C2;
        }
        setSemiTrans(prim, 1);
        setShadeTex(prim, 1);
        u0 = (arg1 & 0xF) * 40;
        u1 = u0 + 0x27;
        setUV4(prim, u0, 0x50, u1, 0x50, u0, 0x77, u1, 0x77);
        block->extent.corner.x = (((arg2 * 39) / block->depth) * rsin(arg3)) >> 12;
        block->extent.corner.y = (((arg2 * 39) / block->depth) * rcos(arg3)) >> 12;
        prim->x0               = block->screenX + block->extent.corner.x;
        prim->x3               = block->screenX - block->extent.corner.x;
        prim->y0               = block->screenY - block->extent.corner.y;
        prim->y3               = block->screenY + block->extent.corner.y;
        ang2                   = arg3 + 0x400;
        block->extent.corner.x = (((arg2 * 39) / block->depth) * rsin(ang2)) >> 12;
        block->extent.corner.y = (((arg2 * 39) / block->depth) * rcos(ang2)) >> 12;
        prim->x1               = block->screenX + block->extent.corner.x;
        prim->x2               = block->screenX - block->extent.corner.x;
        prim->y1               = block->screenY - block->extent.corner.y;
        prim->y2               = block->screenY + block->extent.corner.y;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)block->depth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BLOCK(EffectShapeScratch);
}
