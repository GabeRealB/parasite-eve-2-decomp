#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "weapons/weapon.h"

/// The weapon's index: 0x10 for the M4A1, 0x14 and 0x15 for its two upgrades.
/// The three packages are this source built once each, and each declares its
/// index in the manifest. It keys the firing sound, the shot effect and the item.
#ifndef WEAPON_ID
#error "WEAPON_ID is a per-package build parameter"
#endif

/* gameplay's weapon table names each package's attack handler, so each build of
 * this source gives the handler its own package's name. */
#if WEAPON_ID == 0x14
#define func_m4a1_8011D1C4 func_m4a1_p1_8011D1C4
#elif WEAPON_ID == 0x15
#define func_m4a1_8011D1C4 func_m4a1_p2_8011D1C4
#endif

void func_m4a1_8011D1C4(Task* arg0);

void func_m4a1_8011D1C4(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;
    s32        delay;
    /* Narrower than the field it feeds on purpose: an `s32 shots = 1` would join
       the switch's SImode `1` in the same cse class and steal its register for
       the `field_97F == 1` compare below. */
    s16 shots;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = 1;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = 9;
            shots                    = 1;
            if (actor->attackButton == 1) {
                shots = 3;
            }
            actor->actionValue = shots;
            func_80106238(arg0, 0, actor->attackButton == 1);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0xC00;
            anim                                                  = 1;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 6;
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
            if (actor->actionValue != 0) {
                delay = actor->stateTimer;
                if (delay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = 3;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20000004 | (WEAPON_ID << 16), 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_ID, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                    break;
                }
                actor->stateTimer = delay - 1;
                if (delay - 1 == 2) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                        Gp_PlayObjSfx(spot, 0x17, 1);
                    }
                }
            } else {
                /* The lock-on block is spelled out in both arms, not shared: with
                   one copy after the `if`, cross-jumping merges the `field_12A`
                   load into the tail and drops two instructions. */
                actor->statePhase                                     = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                    Gp_PlayObjSfx(spot, 0x17, 1);
                }
            }
            break;
        case 3:
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

/* Each package carries its own model. */
#if WEAPON_ID == 0x10
static TmdBone _gM4a1Model006ACSkeleton[1] = {
#include "assets/m4a1_model_006AC_skeleton.inc"
};

static u32 _gM4a1Model006ACPartVerts[1] = {
#include "assets/m4a1_model_006AC_partVerts.inc"
};

static SVECTOR _gM4a1Model006ACVerts[58] = {
#include "assets/m4a1_model_006AC_verts.inc"
};

static SVECTOR _gM4a1Model006ACNormals[58] = {
#include "assets/m4a1_model_006AC_normals.inc"
};

static u32 _gM4a1Model006ACStream[406] = {
#include "assets/m4a1_model_006AC_stream.inc"
};

TmdSource D_m4a1_8011DEC4 = {
    0,
    2940,
    0,
    1,
    _gM4a1Model006ACPartVerts,
    _gM4a1Model006ACVerts,
    _gM4a1Model006ACNormals,
    _gM4a1Model006ACSkeleton,
    _gM4a1Model006ACStream,
};
#elif WEAPON_ID == 0x14
static TmdBone _gM4a1P1M4a1Model006ACSkeleton[1] = {
#include "assets/m4a1_model_006AC_skeleton.inc"
};

static u32 _gM4a1P1M4a1Model006ACPartVerts[1] = {
#include "assets/m4a1_model_006AC_partVerts.inc"
};

static SVECTOR _gM4a1P1M4a1Model006ACVerts[58] = {
#include "assets/m4a1_model_006AC_verts.inc"
};

static SVECTOR _gM4a1P1M4a1Model006ACNormals[58] = {
#include "assets/m4a1_model_006AC_normals.inc"
};

static u32 _gM4a1P1M4a1Model006ACStream[406] = {
#include "assets/m4a1_model_006AC_stream.inc"
};

TmdSource D_m4a1_p1_8011DEC4 = {
    0,
    2940,
    0,
    1,
    _gM4a1P1M4a1Model006ACPartVerts,
    _gM4a1P1M4a1Model006ACVerts,
    _gM4a1P1M4a1Model006ACNormals,
    _gM4a1P1M4a1Model006ACSkeleton,
    _gM4a1P1M4a1Model006ACStream,
};
#elif WEAPON_ID == 0x15
static TmdBone _gM4a1P2M4a1Model006ACSkeleton[1] = {
#include "assets/m4a1_model_006AC_skeleton.inc"
};

static u32 _gM4a1P2M4a1Model006ACPartVerts[1] = {
#include "assets/m4a1_model_006AC_partVerts.inc"
};

static SVECTOR _gM4a1P2M4a1Model006ACVerts[58] = {
#include "assets/m4a1_model_006AC_verts.inc"
};

static SVECTOR _gM4a1P2M4a1Model006ACNormals[58] = {
#include "assets/m4a1_model_006AC_normals.inc"
};

static u32 _gM4a1P2M4a1Model006ACStream[406] = {
#include "assets/m4a1_model_006AC_stream.inc"
};

TmdSource D_m4a1_p2_8011DEC4 = {
    0,
    2940,
    0,
    1,
    _gM4a1P2M4a1Model006ACPartVerts,
    _gM4a1P2M4a1Model006ACVerts,
    _gM4a1P2M4a1Model006ACNormals,
    _gM4a1P2M4a1Model006ACSkeleton,
    _gM4a1P2M4a1Model006ACStream,
};
#endif
