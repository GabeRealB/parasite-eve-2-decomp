#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/weapon.h"

/// The PA3 and SP12 shotguns are this source built once each, and each declares
/// these values in the manifest. `WEAPON_ID` is the weapon's index (0xD and
/// 0xE), which keys the sound bank, the shot effect and the item.
/// `PA3_FIELD_979` is the value `field_979` is primed with when the shot is
/// armed.
#if !defined(WEAPON_ID) || !defined(PA3_FIELD_979)
#error "WEAPON_ID and PA3_FIELD_979 are per-package build parameters"
#endif

/* gameplay's weapon table names each package's attack handler, so each build of
 * this source gives the handler its own package's name. */
#if WEAPON_ID == 0xE
#define func_pa3_8011D1DC func_sp12_8011D1DC
#endif

void func_pa3_8011D1DC(Task* arg0);

/// Per-frame firing state machine for the shotgun. Case 0 arms the shot -
/// clearing the recoil counters, priming the `field_979` grace at `PA3_FIELD_979` and the
/// `field_934` frame delay at 0x1F - and queues the ready animation, using the
/// long variant when the weapon was left dirty (`field_958`) or the actor is
/// flagged in `field_975`; the muzzle grip bit in `field_12A` is set only for
/// the 0xE weapon variant. Case 1 waits for that animation to reach its second
/// slot. Case 2 fires, consuming the weapon's item, playing the report and spawning the
/// flash. Case 3 re-acquires the lock-on target, sourcing the impact sound from
/// the actor's own contact point on the 0xE variant. Case 4 runs out the
/// `field_934` delay before playing the pump-action sound, and case 5 runs out
/// the `field_979` grace and otherwise hands back to `func_80106550`.
void func_pa3_8011D1DC(Task* arg0)
{
    GameActor* actor;
    GfxCoord*  coord;
    GfxCoord*  spot;
    s32        anim;
    s32        hit;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot  = SCRATCH_STACK_CURSOR(GfxCoord);
    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->statePhase        = 1;
            actor->attackCancelTicks = PA3_FIELD_979;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0x1F;
            func_80106238(arg0, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~0x800;
            }
            anim = 1;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            /* fallthrough */
        case 1:
            if (Gp_AnimGetRec(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_ConsumeSlotQty(WEAPON_ITEM(WEAPON_ID), 1);
            Gp_PlayObjSfx(arg0->extra.tmd->coords,
                          ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x20000005 | (WEAPON_ID << 16), 1);
            Gp_SpawnEff(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 0x10) | WEAPON_ID, NULL);
            playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 3);
            break;
        case 3:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (gPlayerStatus.weaponSlotItem != 0xD) {
                hit = Gp_PickNearestRec18(actor->weaponContacts, coord, spot);
                if (gPlayerStatus.weaponSlotItem == 0xE) {
                    if (hit != 0 || Gp_CountRec18Hi(actor->weaponContacts, 0x30000) != 0) {
                        spot->workm.t[0] = actor->weaponContacts[0].point.vx;
                        spot->workm.t[1] = actor->weaponContacts[0].point.vy;
                        spot->workm.t[2] = actor->weaponContacts[0].point.vz;
                        Gp_PlayObjSfx(spot,
                                      ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x20000004 | (WEAPON_ID << 16), 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx(spot, 0x17, 1);
                }
            }
            /* fallthrough */
        case 4:
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                Gp_PlayObjSfx(arg0->extra.tmd->coords,
                              ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x20000002 | (WEAPON_ID << 16), 0);
            }
            /* fallthrough */
        case 5:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 1;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

/* Each package carries its own model. */
#if WEAPON_ID == 0xD
static TmdBone _gPa3Model0056CSkeleton[1] = {
#include "assets/pa3_model_0056C_skeleton.inc"
};

static u32 _gPa3Model0056CPartVerts[1] = {
#include "assets/pa3_model_0056C_partVerts.inc"
};

static SVECTOR _gPa3Model0056CVerts[26] = {
#include "assets/pa3_model_0056C_verts.inc"
};

static SVECTOR _gPa3Model0056CNormals[26] = {
#include "assets/pa3_model_0056C_normals.inc"
};

static u32 _gPa3Model0056CStream[182] = {
#include "assets/pa3_model_0056C_stream.inc"
};

TmdSource D_pa3_8011DA04 = {
    0,
    1276,
    0,
    1,
    _gPa3Model0056CPartVerts,
    _gPa3Model0056CVerts,
    _gPa3Model0056CNormals,
    _gPa3Model0056CSkeleton,
    _gPa3Model0056CStream,
};
#elif WEAPON_ID == 0xE
static TmdBone _gSp12Model005CCSkeleton[1] = {
#include "assets/sp12_model_005CC_skeleton.inc"
};

static u32 _gSp12Model005CCPartVerts[1] = {
#include "assets/sp12_model_005CC_partVerts.inc"
};

static SVECTOR _gSp12Model005CCVerts[34] = {
#include "assets/sp12_model_005CC_verts.inc"
};

static SVECTOR _gSp12Model005CCNormals[30] = {
#include "assets/sp12_model_005CC_normals.inc"
};

static u32 _gSp12Model005CCStream[238] = {
#include "assets/sp12_model_005CC_stream.inc"
};

TmdSource D_sp12_8011DB44 = {
    0,
    1692,
    0,
    1,
    _gSp12Model005CCPartVerts,
    _gSp12Model005CCVerts,
    _gSp12Model005CCNormals,
    _gSp12Model005CCSkeleton,
    _gSp12Model005CCStream,
};
#endif
