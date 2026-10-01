#include "weapons/m4a1_pyke.h"

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
#include "gameplay/animation.h"
#include "gameplay/effect_tasks.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/random.h"
#include "main/gfx.h"
#include "main/gfx_types.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/mem.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/pyke_flame.h"

/// 0x38 block the flying dart's spawn state allocates with `memCalloc` and
/// parks in `Task::work`. It leads with the `WorldCollisionBody` list node
/// `func_m4a1_pyke_8011E4AC` hands back to `Gp_UnlinkObj` on teardown; `rec` is
/// the single-entry `WorldCollisionContact` collision table `obj.context.contacts` points at, and its
/// `flags` is set to 2 (the last-element bit) instead of going through
/// `Gp_InitRec18Table`.
typedef struct M4a1PykeBeam {
    /* 0x00 */ WorldCollisionBody    obj;
    /* 0x20 */ WorldCollisionContact rec[1];
} M4a1PykeBeam;
STATIC_ASSERT_SIZEOF(M4a1PykeBeam, 0x38);

/// 0x30-byte scratch from the scratch stack used by `func_m4a1_pyke_8011E168`
/// for the dart's ground splash. `vec` holds the four corners of the unit quad
/// `D_80111E38`, scaled to the splash half-size, rotated flat into view space
/// by `gGfxViewCoord.workm` and translated to `pos`; `sxy` is where they project
/// to on screen, `vec[0]` through a single `RTPS` and the rest through one
/// `RTPT`. Same shape as the gameplay `GpQuadScratch`, but with `otz` and
/// `flag` kept on the stack instead of in the block.
typedef struct M4a1PykeSplashScratch {
    /* 0x00 */ SVECTOR vec[4];
    /* 0x20 */ DVECTOR sxy[4];
} M4a1PykeSplashScratch;
STATIC_ASSERT_SIZEOF(M4a1PykeSplashScratch, 0x30);

static void func_m4a1_pyke_8011E168(VECTOR3* pos, s32 width);

/// Translation of the Pyke's effect coordinate frame inside its parent frame
/// (the muzzle), `(0, 0x200, 0x40)`.
static SVECTOR D_m4a1_pyke_8011E90C = { 0, 0x200, 0x40, 0 };

static void func_m4a1_pyke_8011E4AC(Task* task);

static void func_m4a1_pyke_8011E4F8(Task* arg0);

/// Per-frame beam task for the M4A1 Pyke. Nothing runs while the player model
/// is hidden (`field_C & 0x80`) or effects are hidden
/// (`gRoomEffectState->effectControl >= 2`). State 0 hangs the task's own coordinate off
/// `field_8` at the fixed muzzle offset with an identity rotation; state 1 then
/// dispatches on `spawnArg1`:
///
/// - 1 draws the beam head at `workm.t` every frame and claims room-light slot
///   1 as a narrow (`0x80` / `0x400`) light aimed at a random angle in
///   `0x400..0xB00`, arming the flare width in `scale`.
/// - 2 widens that flare by 0x40 a frame up to 0x180, spawns effect `0x6017F`
///   as a child of this task, and re-claims the light with a much wider
///   (`0x400` / `0x4000`) falloff and a `0x800..0xF00` angle.
/// - 3 and 4 switch back to sub-state 1 and 0, and 5 releases the pool block.
///
/// While `gRoomEffectState->effectControl` is non-zero the two drawing sub-states wind
/// `age` back down instead of advancing.
void func_m4a1_pyke_8011D1F8(Task* task)
{
    EffectWork*           work;
    GfxCoord*             coord;
    GpCoord64*            base;
    WorldCoordPointLight* slot;
    GfxCoord*             light;
    GpMtxWords*           rot;
    EffectWork*           eff;
    u32                   ang;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    base  = &Gp_RoomCoords[1];
    light = &base->light.head.transform.coord;
    slot  = &base->light;
    if ((gameGetPtrSlot(3)->extra.tmd->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW) != 0) {
        return;
    }
    if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_HIDDEN) {
        return;
    }
    work->age++;
    switch (task->state) {
        case 0:
            rot                 = (GpMtxWords*)&coord->coord;
            coord->parent       = work->parent;
            rot->m00_m01        = 0x1000;
            rot->m02_m10        = 0;
            rot->m11_m12        = 0x1000;
            rot->m20_m21        = 0;
            rot->m22            = 0x1000;
            coord->coord.t[0]   = D_m4a1_pyke_8011E90C.vx;
            coord->coord.t[1]   = D_m4a1_pyke_8011E90C.vy;
            coord->coord.t[2]   = D_m4a1_pyke_8011E90C.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            task->state = 1;
            break;
        case 1:
            switch (task->spawnArg1.value) {
                case 0:
                    break;
                case 1:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        pykeFlameDrawNozzle(
                            MATRIX_TRANS(&coord->workm), work->age, 0x80);
                        break;
                    }
                    Gp_UpdateCoord(coord);
                    pykeFlameDrawNozzle(MATRIX_TRANS(&coord->workm), work->age, 0x80);
                    base->framesLeft = 4;
                    slot->inner      = 0x80;
                    slot->outer      = 0x400;
                    ang              = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState  = ang;
                    // Green halves the unsigned red halfword; blue quarters its signed value.
                    slot->head.color.r = ((ang >> 16) & 0x700) + 0x400;
                    slot->head.color.g = (u16)slot->head.color.r >> 1;
                    slot->head.color.b = slot->head.color.r >> 2;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    work->scale         = 0x40;
                    break;
                case 2:
                    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
                        work->age--;
                        break;
                    }
                    Gp_UpdateCoord(coord);
                    if (work->scale < 0x180) {
                        work->scale = work->scale + 0x40;
                    }
                    eff = Gp_SpawnEff(0x6017F, coord, (s32)(work->scale), NULL);
                    if (eff != NULL) {
                        Task_Reparent(task, eff->task);
                    }
                    base->framesLeft   = 4;
                    slot->inner        = 0x400;
                    slot->outer        = 0x4000;
                    ang                = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                    gRandomLcgState    = ang;
                    slot->head.color.r = ((ang >> 16) & 0x700) + 0x800;
                    slot->head.color.g = (u16)slot->head.color.r >> 1;
                    slot->head.color.b = slot->head.color.r >> 2;
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &coord->workm, &light->coord);
                    light->composeStamp = GRAPHICS_COORD_DIRTY;
                    break;
                case 3:
                    task->spawnArg1.value = 1;
                    break;
                case 4:
                    task->spawnArg1.value = 0;
                    break;
                case 5:
                    Gp_ReleaseState1CMem(work, task);
                    break;
            }
            break;
    }
}

#include "../../shared/pyke_flame_nozzle.inc.c"

/// Per-frame task for one dart the Pyke throws. `Task::spawnArg2` is the
/// `EffectWork` holding the dart's velocity (`move` / `move.vy`
/// / `move.vz`), its age (`age`), its flare width (`scale`) and its
/// spin angle (`angle`); `Task::extra` reaches the coordinate the dart flies
/// on. Everything stops on cancellation (`gRoomEffectState->effectControl >=
/// 4`); with nonzero control below that threshold the dart is only redrawn.
///
/// - State 0 allocates the `M4a1PykeBeam` list node, aims the dart by rotating
///   `(0, spawnArg1 - rand(0..0x3F), 0)` through the coordinate's own matrix,
///   seeds the flare width and spin angle, links the node and falls through.
/// - State 1 flies the dart, redraws it, and every third frame traces the
///   ground under it for a splash. Hitting a wall (`Gp_CountRec18Hi`) or living
///   past 0x14 frames releases the block; hitting geometry (`func_800DE7CC`)
///   switches to state 2 with a fresh ricochet velocity.
/// - State 2 coasts on that velocity with a fast-widening flare until the dart
///   is 0x15 frames old.
void func_m4a1_pyke_8011D7D4(Task* task)
{
    GfxCoord      ground;
    SVECTOR       after;
    SVECTOR       before;
    GfxCoord*     coord;
    EffectWork*   work;
    M4a1PykeBeam* beam;
    s32           effectControl;
    u32           ang0;
    u32           ang1;
    u32           ang2;
    u32           ang3;

    beam          = (M4a1PykeBeam*)task->work;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (task->state != 0) {
            Gp_UnlinkObj(&beam->obj);
        }
        Gp_ReleaseState1CMem(work, task);
        return;
    }
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                          (work->age >> 1) + 1, work->scale, work->angle);
        return;
    }
    work->age = work->age + 1;
    switch (task->state) {
        case 0:
            beam = memCalloc(sizeof(M4a1PykeBeam), 0);
            if (beam == NULL) {
                work->age = 0;
                return;
            }
            task->exitCallback = func_m4a1_pyke_8011E4AC;
            /* The three halfwords are the SVECTOR `gte_rtv0` rotates in
               place, so `field_14` has to be cleared after the random pitch is
               written to `field_12`, not alongside `field_10`. */
            work->move.vx   = 0;
            ang0            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = ang0;
            work->move.vy   = (u16)task->spawnArg1.value - ((ang0 >> 16) & 0x3F);
            work->move.vz   = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            work->scale                = (u16)task->spawnArg1.value + 0x180;
            ang1                       = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle                = (ang1 >> 16) & 0xFFF;
            task->state                = 1;
            task->work                 = beam;
            beam->obj.coord            = coord;
            beam->obj.context.contacts = beam->rec;
            beam->obj.key              = 0x21C1E;
            beam->obj.radius           = work->scale >> 1;
            gRandomLcgState            = ang1;
            beam->obj.flags            = WORLD_COLLISION_BODY_SPHERE;
            Gp_LinkObj(1, &beam->obj);
            beam->rec[0].flags = 2;
            beam->obj.flags   |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            /* fallthrough */
        case 1:
            work->scale         = work->scale + 0x10;
            work->move.vy       = work->move.vy + 8;
            before.vx           = coord->workm.t[0];
            before.vy           = coord->workm.t[1];
            before.vz           = coord->workm.t[2];
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            after.vx = coord->workm.t[0];
            after.vy = coord->workm.t[1];
            after.vz = coord->workm.t[2];
            pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                              (work->age >> 1) + 1, work->scale,
                              work->angle);
            ang2            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = ang2;
            if ((u16)((ang2 >> 16) % 3) == 0 && gRoomEffectState->groundTraceEnabled != 0 &&
                Gp_TraceGroundCoord(coord, &ground) == 1) {
                func_m4a1_pyke_8011E168(MATRIX_TRANS(&ground.workm),
                                        (s16)((work->scale * 2) / 3));
            }
            if (Gp_CountRec18Hi(beam->obj.context.contacts, 0x30000) != 0) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            if (func_800DE7CC(&after, &before, NULL, NULL) == 1) {
                Gp_UnlinkObj(&beam->obj);
                task->state     = 2;
                work->move.vx   = (u32)rcos(work->angle) >> 8;
                work->move.vy   = (u32)rsin(work->angle) >> 8;
                ang3            = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = ang3;
                work->move.vz   = (u32)rsin((ang3 >> 16) & 0xFFF) >> 8;
                return;
            }
            if (work->age >= 0x15) {
                Gp_UnlinkObj(&beam->obj);
                Gp_ReleaseState1CMem(work, task);
                return;
            }
            Gp_ClearRec18Occupied(beam->rec);
            return;
        case 2:
            work->scale         = work->scale + 0x40;
            coord->coord.t[0]  += work->move.vx;
            coord->coord.t[1]  += work->move.vy;
            coord->coord.t[2]  += work->move.vz;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                              (work->age >> 1) + 1, work->scale,
                              work->angle);
            if (work->age >= 0x15) {
                Gp_ReleaseState1CMem(work, task);
            }
            break;
    }
}

#include "../../shared/pyke_flame_blob.inc.c"

/// Draws the dart's ground splash: the unit quad `D_80111E38` scaled to
/// `width` half-size, laid flat by `gGfxViewCoord.workm` and moved to the traced
/// ground point `pos`, then projected through `GsWSMATRIX` into a 0x30-byte
/// scratch stack block. The first corner goes through `rtps` and the other
/// three through one `rtpt`; a negative `gte_stflg` drops the quad.
static void func_m4a1_pyke_8011E168(VECTOR3* pos, s32 width)
{
    u8*                    head;
    M4a1PykeSplashScratch* block;
    POLY_FT4*              prim;
    GpQuadCorner*          tbl;
    s32                    i;
    s32                    flag;
    s32                    otz;

    head = SCRATCH_STACK_CURSOR(u8) - 0x30;
    /* The ROM stores the freshly computed head and keeps a *copy* of it in the
       register the rest of the function walks; without the barrier GCC folds
       the two together and stores the copy instead. */
    SCRATCH_STACK_CURSOR(u8) = head;
    block                    = (M4a1PykeSplashScratch*)head;
    gte_SetTransMatrix(&GsWSMATRIX);
    i   = 0;
    tbl = D_80111E38;
    do {
        block->vec[i].vx = tbl[i].x * width;
        block->vec[i].vy = 0;
        block->vec[i].vz = tbl[i].y * width;
        gte_SetRotMatrix(&gGfxViewCoord.workm);
        gte_ldv0(&block->vec[i]);
        gte_rtv0();
        gte_stsv(&block->vec[i]);
        (u16) block->vec[i].vx = (u16)block->vec[i].vx + (u16)pos->vx;
        (u16) block->vec[i].vy = (u16)block->vec[i].vy + (u16)pos->vy;
        (u16) block->vec[i].vz = (u16)block->vec[i].vz + (u16)pos->vz;
        i++;
    } while (i < 4);

    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec[0]);
    gte_rtps();
    gte_stsxy(&block->sxy[0]);
    gte_ldv3(&block->vec[1], &block->vec[2], &block->vec[3]);
    gte_rtpt();
    gte_stsxy3(&block->sxy[1], &block->sxy[2], &block->sxy[3]);
    gte_stflg(&flag);
    if (flag >= 0) {
        /* Dead in the ROM too: `otz` is bumped before it is read back, so the
           increment lands on garbage and `gte_stszotz` immediately overwrites
           it. It still costs a `lw`/`addiu`/`sw` because the address escapes
           into the asm below. */
        otz++;
        gte_stszotz(&otz);
        prim           = gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2E);
        setRGB0(prim, 0x40, 0x40, 0x40);
        prim->tpage = 0x29;
        prim->clut  = 0x430F;
        setUV4(prim, 0xE0, 0xC8, 0xFF, 0xC8, 0xE0, 0xE7, 0xFF, 0xE7);
        prim->x0 = block->sxy[0].vx;
        prim->y0 = block->sxy[0].vy;
        prim->x1 = block->sxy[1].vx;
        prim->y1 = block->sxy[1].vy;
        prim->x2 = block->sxy[2].vx;
        prim->y2 = block->sxy[2].vy;
        prim->x3 = block->sxy[3].vx;
        prim->y3 = block->sxy[3].vy;
        addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
                prim);
    }
    SCRATCH_STACK_RELEASE_BYTES(0x30);
}

/// Exit callback: unlinks the collision node leading `Task::work`, if one was
/// linked, and releases the `EffectWork` in `Task::spawnArg2`.
/// Hypervelocity carries an identical copy.
static void func_m4a1_pyke_8011E4AC(Task* task)
{
    WorldCollisionBody* obj = task->work;
    void*               mem = task->spawnArg2.pointer;

    if (obj != NULL) {
        Gp_UnlinkObj(obj);
    }
    Gp_ReleaseState1CMem(mem, task);
}

/// Per-frame firing state machine for the M4A1 Pyke. State 0 arms the shot and
/// raises the weapon (clip 8 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: a held trigger (bit 0)
/// drops into the three-round burst of state 3, a tap (bit 1) launches the dart
/// (state 5) after telling the beam task (`field_914`) to go to sub-state 2,
/// and anything else falls straight into the burst. State 3 counts `field_934`
/// down to each round, spending one magazine round, playing `0x201C0004` and
/// spawning the muzzle flash, and picks the lock-on target on the frame after.
/// State 4 picks that target once and hands over to state 6. State 5 waits out
/// `field_93E` and then asks `func_801060E0` where the dart went: a hit
/// (`2`) with rounds still to spend rearms for another `0x14` frames, anything
/// else ends the burst, parks the beam task at sub-state 3 or 4 and plays the
/// `0x201C0005` tail. State 6 counts `field_979` down and drops out of the
/// firing pose once the aim check fails or the trigger has been released.
static void func_m4a1_pyke_8011E4F8(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    Task*      beam;
    s32        anim;
    s32        delay;
    s32        spent;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot = SCRATCH_STACK_CURSOR(GfxCoord);
    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            Gp_AnimPlayChildSlotsEx(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->rumblePosted = 0;
            if (actor->attackButton & 1) {
                actor->statePhase        = 3;
                actor->turnRateIndex     = 0;
                actor->stateTimer        = 0;
                actor->attackCancelTicks = 9;
                actor->actionValue       = 3;
                func_80106238(arg0, 0, 1);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            } else if (actor->attackButton & 2) {
                beam                               = actor->weaponEffectTask;
                actor->statePhase                  = 5;
                actor->turnRateIndex               = 2;
                actor->attackControl.cooldownTicks = 0x28;
                actor->attackCancelTicks           = 0x1C;
                actor->actionValue                 = 0x14;
                if (beam != NULL) {
                    beam->spawnArg1.value = 2;
                }
                Gp_ConsumeSlotQty(0x9B, 0x101);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201C0005, 1);
                Gp_AnimPlayChildSlotsEx(arg0, 0xB, 0, 2);
                break;
            }
            /* fallthrough */
        case 3:
            if (actor->actionValue != 0) {
                delay = actor->stateTimer;
                if (delay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = 3;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC000;
                    Gp_ConsumeSlotQty(0x9B, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x201C0004, 1);
                    Gp_SpawnEff(0x6006B,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x1C, NULL);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xA, 0, 2);
                    break;
                }
                actor->stateTimer = delay - 1;
                if (delay - 1 == 2) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0x3FFF;
                    if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                        Gp_PlayObjSfx(spot, 0x17, 1);
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0x3FFF;
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            break;
        case 5:
            if (actor->actionValue == 0) {
                spent = func_80106264(2);
                if ((s8)func_801060E0(arg0) == 2 && spent != 0) {
                    actor->actionValue = 0x14;
                    Gp_ConsumeSlotQty(0x9B, 0x101);
                } else {
                    beam              = actor->weaponEffectTask;
                    actor->statePhase = 6;
                    if (beam != NULL) {
                        beam->spawnArg1.value = spent != 0 ? 3 : 4;
                    }
                    SndEvt_EnqueueType7(0x201C0005, 1);
                    Gp_AnimPlayChildSlotsEx(arg0, 0xF, 0, 2);
                }
            } else {
                actor->actionValue = (u16)actor->actionValue - 1;
            }
            break;
        case 6:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}
