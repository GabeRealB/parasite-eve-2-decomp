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

void func_as12_8011D1DC(Task* arg0);

/// Per-frame firing state machine for the AS12 automatic shotgun. Case 0 arms
/// the shot and queues the ready animation, choosing the long variant when the
/// weapon was left dirty (`field_958`) or the actor is flagged in `field_975`;
/// the muzzle-flash grip bit in `field_12A` is set only for the 0xE weapon
/// variant. Case 1 waits for that animation to reach its second slot. Case 2
/// fires - consuming ammo 0x8E, playing the report and spawning the flash -
/// and case 4 re-acquires the lock-on target, sourcing the impact sound from
/// the actor's own contact point on the 0xE variant. Case 5 runs out the
/// `field_979` grace, re-fires while the trigger is held and otherwise hands
/// back to `func_80106550`.
void func_as12_8011D1DC(Task* arg0)
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
            actor->state          = 4;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            if (gPlayerStatus.weaponSlotItem == 0xE) {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
            } else {
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= ~0x800;
            }
            anim = 1;
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
            actor->statePhase                  = 3;
            actor->attackControl.cooldownTicks = 0x21;
            actor->rumblePosted                = 0;
            func_80106238(arg0, 0, actor->attackButton != 1);
            /* fallthrough */
        case 3:
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            Gp_ConsumeSlotQty(0x8E, 1);
            Gp_PlayObjSfx(arg0->extra.tmd->coords,
                          ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x200F0005, 1);
            Gp_SpawnEff(EFFECT_SHOTGUN_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        (gPlayerStatus.weaponSlotItem << 0x10) | 0xF, NULL);
            Gp_AnimResetChildSlots(arg0, 0xA);
            break;
        case 4:
            actor->attackCancelTicks = 0x16;
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
                                      ((gPlayerStatus.weaponSlotItem - 0xD) << 0x18) | 0x200F0004, 1);
                    }
                } else if (hit != 0) {
                    Gp_PlayObjSfx(spot, 0x17, 1);
                }
            }
            /* fallthrough */
        case 5:
            if ((s8)func_801060E0(arg0) != 0 && func_80106264(1) > 0 && actor->attackControl.cooldownTicks == 0) {
                goto fire;
            }
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}

static TmdBone _gAs12Model00660Skeleton[1] = {
#include "assets/as12_model_00660_skeleton.inc"
};

static u32 _gAs12Model00660PartVerts[1] = {
#include "assets/as12_model_00660_partVerts.inc"
};

static SVECTOR _gAs12Model00660Verts[46] = {
#include "assets/as12_model_00660_verts.inc"
};

static SVECTOR _gAs12Model00660Normals[38] = {
#include "assets/as12_model_00660_normals.inc"
};

static u32 _gAs12Model00660Stream[308] = {
#include "assets/as12_model_00660_stream.inc"
};

TmdSource D_as12_8011DCF0 = {
    0,
    2212,
    0,
    1,
    _gAs12Model00660PartVerts,
    _gAs12Model00660Verts,
    _gAs12Model00660Normals,
    _gAs12Model00660Skeleton,
    _gAs12Model00660Stream,
};
