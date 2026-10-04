#include "weapons/tonfa_baton.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "common.h"
#include "gte.h"

#include "tonfa_baton_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/effects.h"
#include "gameplay/hud_sprites.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gfx.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "../../shared/blade_trail.h"

/// Divisor that turns the model's forward axis into the distance a baton
/// strike carries the actor per frame: 4096 / 84, about 48 coordinate units.
enum { TONFA_BATON_ATTACK_ADVANCE_DIVISOR = 84 };

/// Scratch-stack block for the tonfa baton's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages the forward step a strike
/// moves the actor by.
///
/// `forward` is read from the root coordinate's local matrix and is not
/// normalized. `advance` is each component of it divided by
/// `TONFA_BATON_ATTACK_ADVANCE_DIVISOR` and multiplied by 1 on a frame a
/// strike carries the actor forward, 0 on any other. It is added to the root
/// coordinate's local translation in every state. Neither vector's `pad` is
/// written.
typedef struct {
    VECTOR  advance; // This frame's displacement of the model's root coordinate, in coordinate units; zero while no strike carries the actor
    SVECTOR forward; // Model's forward axis: the Z column of its root coordinate's local matrix, 4096 per unit
} _TonfaBatonAttackScratch;
STATIC_ASSERT_SIZEOF(_TonfaBatonAttackScratch, 0x18);

/// The near end of the baton trail inside the weapon frame; the task's own
/// coordinate starts there. The far end follows it directly, and state 0 reaches
/// that as element 1 of this array.
static SVECTOR D_tonfa_baton_8011E0F0[1] = { { 0, 0x0080, 0, 0 } };

/// The far end of that pair, immediately after it. Both forms appear in
/// the original: one path reaches it as `D_tonfa_baton_8011E0F0[1]`, which compiles to the
/// array's address plus 8, and another names it directly, which compiles
/// to its own address - so it has to be a separate object, not element 1.
static SVECTOR D_tonfa_baton_8011E0F8 = { 0, -0x0200, 0, 0 };

static void func_tonfa_baton_8011DB78(Task* task);

static void func_tonfa_baton_8011DA48(Task* task);
static void func_tonfa_baton_8011DA74(Task* arg0);
static void func_tonfa_baton_8011DB6C(Task* arg0);
void        func_tonfa_baton_8011DBFC(Task* arg0);

void func_tonfa_baton_8011D1EC(Task* task)
{
    GfxCoord    local;
    GfxCoord*   coord;
    GfxCoord*   dst;
    EffectWork* work;
    SVECTOR*    vec;
    s32         i;
    s32         flags;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        work->age++;
        switch (task->state) {
            case 0:
                coord->parent       = work->parent;
                coord->coord.t[0]   = D_tonfa_baton_8011E0F0[0].vx;
                coord->coord.t[1]   = D_tonfa_baton_8011E0F0[0].vy;
                coord->coord.t[2]   = D_tonfa_baton_8011E0F0[0].vz;
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                task->state        = 1;
                vec                = &D_tonfa_baton_8011E0F0[1];
                local.parent       = coord;
                local.coord.t[0]   = vec->vx;
                local.coord.t[1]   = vec->vy;
                local.coord.t[2]   = vec->vz;
                local.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&local);
                for (i = 0; i < 8; i++) {
                    dst         = &gBladeTrailBase[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = coord->workm;
                    gte_SetRotMatrix(&coord->workm);
                    gte_SetTransMatrix(&coord->workm);
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst         = &gBladeTrailTip[i];
                    dst->parent = &gGfxViewCoord;
                    dst->workm  = local.workm;
                    gte_SetRotMatrix(&local.workm);
                    gte_SetTransMatrix(&local.workm);
                    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                flags = 0x13;
                if (task->spawnArg1.value == 0) {
                    flags = 1;
                }
                D_tonfa_baton_8012C0EC = flags;
                break;
            case 1:
                coord->composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(coord);
                local.parent       = work->parent;
                local.coord.t[0]   = D_tonfa_baton_8011E0F8.vx;
                local.coord.t[1]   = D_tonfa_baton_8011E0F8.vy;
                local.coord.t[2]   = D_tonfa_baton_8011E0F8.vz;
                local.composeStamp = GRAPHICS_COORD_DIRTY;
                Gp_UpdateCoord(&local);
                dst         = &gBladeTrailBase[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = coord->workm;
                gte_SetRotMatrix(&coord->workm);
                gte_SetTransMatrix(&coord->workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst         = &gBladeTrailTip[work->age & 7];
                dst->parent = &gGfxViewCoord;
                dst->workm  = local.workm;
                gte_SetRotMatrix(&local.workm);
                gte_SetTransMatrix(&local.workm);
                gfxMakeRelativeTransform(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst               = &gBladeTrailBase[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                    dst               = &gBladeTrailTip[i];
                    dst->composeStamp = GRAPHICS_COORD_DIRTY;
                    Gp_UpdateCoord(dst);
                }
                bladeTrailDraw(work->age & 7, D_tonfa_baton_8012C0EC);
                break;
        }
        if (work->age >= 0x1F) {
            effectKillTask(work, task);
        }
    }
}

#include "../../shared/blade_trail_draw.inc.c"

static void func_tonfa_baton_8011DA48(Task* task)
{
    TmdObject* extra;
    GfxCoord*  coord;

    extra               = task->extra.tmd;
    coord               = extra->coords;
    task->state         = task->state + 1;
    task->exitCallback  = func_tonfa_baton_8011DB78;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = 0;
}

static void func_tonfa_baton_8011DA74(Task* arg0)
{
    TmdObject* extra;
    GfxCoord*  coord;
    GameActor* actor;
    s32        mode;

    extra               = arg0->extra.tmd;
    coord               = extra->coords;
    actor               = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->work;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    extra->flags        = (gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->flags;

    coord->coord.t[0] = 0;
    coord->coord.t[1] = 0x60;
    coord->coord.t[2] = 0;

    if (*(u32*)&actor->mode != 0x40000) {
        arg0->spawnArg1.value = 0;
    }

    mode = arg0->spawnArg1.value & 0xF;
    switch (mode) {
        case 0:
            if (coord->param.rot.vz > 0) {
                coord->param.rot.vz = coord->param.rot.vz - 0x100;
            }
            break;
        case 1:
            if (coord->param.rot.vz < 0x800) {
                coord->param.rot.vz = coord->param.rot.vz + 0x1C0;
            }
            break;
    }
    gfxRotMatrixZ(&coord->coord, coord->param.rot.vz, GRAPHICS_ROTATION_REPLACE);
}

static void func_tonfa_baton_8011DB6C(Task* arg0)
{
    arg0->state = 3;
}

/// Exit callback: kills the task.
static void func_tonfa_baton_8011DB78(Task* task)
{
    taskKill(task);
}

/// Per-frame entry point: runs the weapon task's current state. The table is a
/// local, so GCC copies it from `.rodata` onto the stack every frame.
void func_tonfa_baton_8011DB98(Task* arg0)
{
    TaskFunc states[4] = {
        func_tonfa_baton_8011DA48,
        func_tonfa_baton_8011DA74,
        func_tonfa_baton_8011DB6C,
        func_tonfa_baton_8011DB78,
    };

    states[arg0->state](arg0);
}

/// Per-frame swing state machine for the tonfa baton. Its tail is common to
/// every state: it reads the model's forward axis out of its root coordinate's
/// matrix and, only on a frame that set `swinging`, moves that coordinate
/// forward by a `TONFA_BATON_ATTACK_ADVANCE_DIVISOR`th of it, which is what
/// carries the lunge. Case 0 arms the swing (8-tick wind-up) and queues the
/// ready animation. Cases 1 and 2 run the wind-up: on
/// the tick it expires the weapon becomes solid, the swing report plays and the
/// trail effect is parented to the weapon task; pressing again during the
/// window (`field_966 & 0xA`) upgrades to the second swing, which case 2 turns
/// into the follow-through, otherwise the state falls back to the 10-tick
/// recovery of case 5. Case 3 is the follow-through: it re-arms the hitbox
/// three ticks in and parks in case 4, whose 9 ticks clear the hit flag again.
/// Cases 1/2 and 4 also play the connect sound once per swing when
/// `Gp_CountRec18Hi` reports a hit.
void func_tonfa_baton_8011DBFC(Task* arg0)
{
    GameActor*                actor;
    GfxCoord*                 coord;
    _TonfaBatonAttackScratch* scratch;
    EffectWork*               eff;
    s32                       delay;
    s32                       step;
    s32                       fade;
    s32                       swinging;

    swinging = 0;
    actor    = arg0->work;
    scratch  = SCRATCH_STACK_RESERVE_BLOCK(_TonfaBatonAttackScratch);
    switch (actor->statePhase) {
        case 0:
            actor->state          = 4;
            actor->statePhase     = 1;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->movementMode   = 0;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->stateTimer     = 8;
            actor->actionValue    = 0;
            func_80106518(0x13);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key   = 0x21317;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = (actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags & 0xF7FF) | 0x400;
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 3);
            break;
        case 1:
        case 2:
            delay = actor->stateTimer;
            if (delay == 0) {
                actor->movementSign = 1;
                swinging            = 1;
                if (actor->padPressed & 0xA) {
                    actor->statePhase = 2;
                }
            } else {
                delay--;
                actor->stateTimer = delay;
                if (delay == 0) {
                    actor->equipmentTasks[1]->spawnArg1.value             = 1;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    func_80106238(arg0, 0, 0);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130001, 0);
                    eff = Gp_SpawnEff(EFFECT_TONFA_BATON_SWING_TRAIL,
                                      actor->equipmentTasks[1]->extra.tmd->coords,
                                      0, NULL);
                    if (eff != NULL) {
                        taskReparent(actor->equipmentTasks[1], eff->task);
                    }
                }
            }
            if (actor->actionValue != 1 && Gp_CountRec18Hi(actor->weaponContacts, 0x30000) != 0) {
                actor->actionValue = 1;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130003, 0);
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                if (actor->statePhase == 2) {
                    actor->statePhase = 3;
                    actor->stateTimer = 0xC;
                    func_80106518(0x13);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key = 0x21315;
                    eff                                                = Gp_SpawnEff(
                        EFFECT_TONFA_BATON_SWING_TRAIL, actor->equipmentTasks[1]->extra.tmd->coords, 1,
                        NULL);
                    if (eff != NULL) {
                        taskReparent(actor->equipmentTasks[1], eff->task);
                    }
                    Gp_AnimResetChildSlots(arg0, 0xB);
                } else {
                    actor->statePhase                         = 5;
                    actor->stateTimer                         = 0xA;
                    actor->equipmentTasks[1]->spawnArg1.value = 0;
                    Gp_AnimResetChildSlots(arg0, 0xE);
                }
            }
            break;
        case 3:
            if (actor->stateTimer != 0) {
                actor->movementSign = 1;
                swinging            = 1;
                step                = actor->stateTimer - 1;
                actor->stateTimer   = step;
                if (step == 3) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                    func_80106238(arg0, 0, 1);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130002, 0);
                } else if (step == 0) {
                    actor->statePhase                         = 4;
                    actor->stateTimer                         = 9;
                    actor->equipmentTasks[1]->spawnArg1.value = 0;
                }
            }
            /* fallthrough */
        case 4:
            if (actor->statePhase == 4) {
                fade = actor->stateTimer;
                fade--;
                actor->stateTimer = fade;
                if (fade == 0) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
                }
            }
            if (actor->actionValue != 2 && Gp_CountRec18Hi(actor->weaponContacts, 0x30000) != 0) {
                actor->actionValue = 2;
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20130004, 0);
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
        case 5:
            if (actor->stateTimer != 0) {
                actor->movementSign = 1;
                swinging            = 1;
                actor->stateTimer   = actor->stateTimer - 1;
            }
            if (func_80105894(arg0, 1, 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    coord = arg0->extra.tmd->coords;
    gfxReadMatrixZAxis(&coord->coord, &scratch->forward);
    scratch->advance.vx = (s16)(scratch->forward.vx / TONFA_BATON_ATTACK_ADVANCE_DIVISOR) * swinging;
    scratch->advance.vy = (s16)(scratch->forward.vy / TONFA_BATON_ATTACK_ADVANCE_DIVISOR) * swinging;
    scratch->advance.vz = (s16)(scratch->forward.vz / TONFA_BATON_ATTACK_ADVANCE_DIVISOR) * swinging;
    coord->coord.t[0]  += scratch->advance.vx;
    coord->coord.t[1]  += scratch->advance.vy;
    coord->coord.t[2]  += scratch->advance.vz;
    SCRATCH_STACK_RELEASE_BLOCK(_TonfaBatonAttackScratch);
}
