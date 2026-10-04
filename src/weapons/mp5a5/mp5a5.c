#include "weapons/mp5a5.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#include "gte.h"
#include "types.h"

#include "mp5a5_private.h"

#include "gameplay/display.h"
#include "gameplay/actor_render.h"
#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/light.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/display.h"
#include "main/display_types.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "overlay.h"

#include "weapons/weapon.h"

#include "../../shared/muzzle_flash.h"

/// The weapon's index: 0x1E for the MP5A5, 0x1F and 0x20 for its two upgrades.
/// The three packages are this source built once each, and each declares its
/// index in the manifest. It keys the firing sounds, the shot id and the item.
#ifndef WEAPON_ID
#error "WEAPON_ID is a per-package build parameter"
#endif

/* gameplay's weapon table names each package's attack handler, so each build of
 * this source gives the handler its own package's name. */
#if WEAPON_ID == 0x1F
#define func_mp5a5_8011DDA4 func_mp5a5_p1_8011DDA4
#elif WEAPON_ID == 0x20
#define func_mp5a5_8011DDA4 func_mp5a5_p2_8011DDA4
#endif

/// Muzzle offset of the weapon, in the firing hand's coordinate frame.
static SVECTOR _gMuzzleOffset = { 0, 0x240, 0x40, 0 };

void func_mp5a5_8011DDA4(Task* arg0);

#include "../../shared/muzzle_flash_task.inc.c"

/// The MP5A5 and its upgrades\'s muzzle-flash task, named by gameplay\'s effect table.
void func_mp5a5_8011D1E0(Task* task)
{
    muzzleFlashTask(task);
}

/// Draws the core of a gun's muzzle flash: one semi-transparent, shade-blended
/// `POLY_FT4` billboarded on `arg0`'s world position. `arg1` is the flash size
/// (scaled down by the projected depth) and `arg2` its spin, so the quad is a
/// square rotated by `arg2` rather than an axis-aligned sprite.
/* `otzp` is a second name for the same block on purpose: `gte_stszotz` takes
   its address in a register of its own, so the ROM keeps a `move` the single
   pointer would have coalesced away. The `gte_ldv0` / `gte_stsxy` addresses
   and every `otz` reload are spelled out from `head` for the same reason -
   off `blk` they would reuse the block register instead. */

/* Every scratch vector address is computed off `head`, not off `blk`, so the
   loads and stores keep spelling the block out from `head` rather than reusing
   the `blk` register the way CSE off `blk` would. */

#include "../../shared/muzzle_flash_core.inc.c"

#include "../../shared/muzzle_flash_streak.inc.c"

/// Per-frame firing state machine for the MP5A5 and its upgrades. State 0 arms the shot and
/// starts the raise animation (clip 5 instead of 1 when the weapon was already
/// up), state 1 waits for that clip, and states 2/3 count `field_934` down to
/// the frame the round leaves the barrel. That frame branches on `field_97F`:
/// single fire (`== 1`) spends one round, plays sound 4 of the weapon's bank, spawns the plain
/// muzzle flash and runs the recoil clip, while burst fire spends 0x101, plays
/// sound 5, holds the pose for 0x12 frames and reparents the longer flash
/// effect under the weapon task. States 4/5 pick the lock-on target once (only
/// while still below 6) and state 6 loops back to `fire` while the trigger is
/// held, the ammo check passes and the burst timer has run out.
void func_mp5a5_8011DDA4(Task* arg0)
{
    GameActor*             actor;
    GfxCoord*              coord;
    GfxCoord*              spot;
    WorldCollisionCapsule* rec;
    EffectWork*            eff;
    s32                    anim;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state                                          = 4;
            actor->turnRateIndex                                  = 2;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState                                 = 0;
            actor->stateTimer                                     = 1;
            actor->statePhase                                    += 1;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            anim                                                  = 1;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 5;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
        fire:
            actor->statePhase   = 3;
            actor->rumblePosted = 0;
            /* fallthrough */
        case 3:
            if (--actor->stateTimer == 0) {
                rec = &actor->weaponShape;
                if (actor->attackButton == 1) {
                    actor->statePhase                                     = 4;
                    actor->stateTimer                                     = 3;
                    actor->attackControl.cooldownTicks                    = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | 0x20000 | (WEAPON_ID << 8);
                    rec->end0Radius                                       = rec->end1Radius;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
                    func_80106238(arg0, 0, 1);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000004 | (WEAPON_ID << 16), 1);
                    Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_ID, NULL);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                } else {
                    actor->statePhase                                     = 5;
                    actor->attackControl.cooldownTicks                    = 0x12;
                    actor->stateTimer                                     = 0x12;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = 0x20016 | (WEAPON_ID << 8);
                    rec->end0Radius                                       = 0xC00;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                    func_80106238(arg0, 0, 0);
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000005 | (WEAPON_ID << 16), 0);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 0x101);
                    eff = Gp_SpawnEff(EFFECT_MP5A5_ALT_FIRE_MUZZLE_FLASH,
                                      actor->equipmentTasks[1]->extra.tmd->coords,
                                      WEAPON_ID, NULL);
                    if (eff != NULL) {
                        taskReparent(actor->equipmentTasks[1], eff->task);
                    }
                }
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            break;
        case 4:
        case 5:
            if (actor->statePhase < 5 && Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            actor->statePhase                                     = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            /* fallthrough */
        case 6:
            if ((s8)func_801060E0(arg0) == 1 && func_80106264(1) > 0 && actor->attackControl.cooldownTicks == 0) {
                goto fire;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                func_80106550(arg0);
            }
            break;
    }
    Gp_TrackLockTarget(arg0);
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

/* Each package carries its own model. */
#if WEAPON_ID == 0x1E
static TmdBone _gMp5a5Model01318Skeleton[1] = {
#include "assets/mp5a5_model_01318_skeleton.inc"
};

static u32 _gMp5a5Model01318PartVerts[1] = {
#include "assets/mp5a5_model_01318_partVerts.inc"
};

static SVECTOR _gMp5a5Model01318Verts[54] = {
#include "assets/mp5a5_model_01318_verts.inc"
};

static SVECTOR _gMp5a5Model01318Normals[58] = {
#include "assets/mp5a5_model_01318_normals.inc"
};

static u32 _gMp5a5Model01318Stream[393] = {
#include "assets/mp5a5_model_01318_stream.inc"
};

TmdSource D_mp5a5_8011EAFC = {
    0,
    2824,
    0,
    1,
    _gMp5a5Model01318PartVerts,
    _gMp5a5Model01318Verts,
    _gMp5a5Model01318Normals,
    _gMp5a5Model01318Skeleton,
    _gMp5a5Model01318Stream,
};
#elif WEAPON_ID == 0x1F
static TmdBone _gMp5a5P1Mp5a5Model01318Skeleton[1] = {
#include "assets/mp5a5_model_01318_skeleton.inc"
};

static u32 _gMp5a5P1Mp5a5Model01318PartVerts[1] = {
#include "assets/mp5a5_model_01318_partVerts.inc"
};

static SVECTOR _gMp5a5P1Mp5a5Model01318Verts[54] = {
#include "assets/mp5a5_model_01318_verts.inc"
};

static SVECTOR _gMp5a5P1Mp5a5Model01318Normals[58] = {
#include "assets/mp5a5_model_01318_normals.inc"
};

static u32 _gMp5a5P1Mp5a5Model01318Stream[393] = {
#include "assets/mp5a5_model_01318_stream.inc"
};

TmdSource D_mp5a5_p1_8011EAFC = {
    0,
    2824,
    0,
    1,
    _gMp5a5P1Mp5a5Model01318PartVerts,
    _gMp5a5P1Mp5a5Model01318Verts,
    _gMp5a5P1Mp5a5Model01318Normals,
    _gMp5a5P1Mp5a5Model01318Skeleton,
    _gMp5a5P1Mp5a5Model01318Stream,
};
#elif WEAPON_ID == 0x20
static TmdBone _gMp5a5P2Mp5a5Model01318Skeleton[1] = {
#include "assets/mp5a5_model_01318_skeleton.inc"
};

static u32 _gMp5a5P2Mp5a5Model01318PartVerts[1] = {
#include "assets/mp5a5_model_01318_partVerts.inc"
};

static SVECTOR _gMp5a5P2Mp5a5Model01318Verts[54] = {
#include "assets/mp5a5_model_01318_verts.inc"
};

static SVECTOR _gMp5a5P2Mp5a5Model01318Normals[58] = {
#include "assets/mp5a5_model_01318_normals.inc"
};

static u32 _gMp5a5P2Mp5a5Model01318Stream[393] = {
#include "assets/mp5a5_model_01318_stream.inc"
};

TmdSource D_mp5a5_p2_8011EAFC = {
    0,
    2824,
    0,
    1,
    _gMp5a5P2Mp5a5Model01318PartVerts,
    _gMp5a5P2Mp5a5Model01318Verts,
    _gMp5a5P2Mp5a5Model01318Normals,
    _gMp5a5P2Mp5a5Model01318Skeleton,
    _gMp5a5P2Mp5a5Model01318Stream,
};
#endif
