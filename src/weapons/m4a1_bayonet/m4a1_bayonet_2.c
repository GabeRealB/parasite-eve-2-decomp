#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/effects.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/session_types.h"
#include "main/task.h"
#include "main/task_types.h"
#include "main/tmd_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

/// One-based weapon index passed to the resident use counter.
enum { M4A1_BAYONET_WEAPON_ID = 26 };

void func_m4a1_bayonet_8011DA34(Task* arg0);

/// Per-frame firing state machine for the M4A1 bayonet. State 0 arms the shot
/// and raises the weapon (clip 8 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: a held trigger (bit 0)
/// drops into the three-round burst of state 3, a tap (bit 1) starts the bayonet
/// thrust of state 5, and anything else falls straight into the burst. State 3
/// counts `field_934` down to each round, spending one magazine round, playing
/// `0x201A0004` and spawning the muzzle flash, and picks the lock-on target on
/// the frame after. State 4 picks that target once and hands over to state 7.
/// States 5/6 run the thrust: `field_93E` times each swing, state 5 hands to
/// state 6 with the `0x201A0006` lunge and its reparented effect, and a hit on
/// any occupied `field_32C` slot plays `0x201A0005`. State 7 counts `field_979`
/// down and drops out of the firing pose once the aim check fails or the
/// trigger has been released.
void func_m4a1_bayonet_8011DA34(Task* arg0)
{
    GameActor*             actor;
    GfxCoord*              coord;
    GfxCoord*              spot;
    WorldCollisionCapsule* rec;
    EffectWork*            eff;
    s32                    anim;
    s32                    delay;
    s16                    frames;

    actor = arg0->work;
    coord = arg0->extra.tmd->coords;
    rec   = &actor->weaponShape;
    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot = SCRATCH_STACK_CURSOR(GfxCoord);
    switch (actor->statePhase) {
        case 0:
            anim                                                  = 1;
            actor->state                                          = 4;
            actor->mode                                           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex                                  = 0;
            actor->animationState                                 = 0;
            actor->statePhase                                    += anim;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x400;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                anim = 8;
            }
            playerActorPlayChildSlotsWithBlend(arg0, 9, 0, anim);
            actor->movementMode = 0;
            break;
        case 1:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case 2:
            actor->rumblePosted = 0;
            if (actor->attackButton & 1) {
                actor->statePhase                                     = 3;
                actor->stateTimer                                     = 0;
                actor->attackCancelTicks                              = 9;
                actor->actionValue                                    = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | 0x21A00;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
                playerActorSetWeaponAttackFlags(arg0, 0, 1);
            } else if (actor->attackButton & 2) {
                actor->statePhase        = 5;
                actor->attackCancelTicks = 0xA;
                actor->actionValue       = 0x12;
                weaponRecordUse(M4A1_BAYONET_WEAPON_ID);
                actor->attackControl.cooldownTicks                    = 0x1C;
                rec->ends[0].vz                                       = rec->ends[1].vz + 0x340;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = 0x21A1D;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                playerActorSetWeaponAttackFlags(arg0, 0, 0);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 1, 3);
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
                    rec->ends[0].vz                                       = rec->ends[1].vz + D_80112F60[26];
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(0x99, EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(arg0->extra.tmd->coords, 0x201A0004, 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x1A, NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 0, 2);
                    break;
                }
                actor->stateTimer = delay - 1;
                if (delay - 1 == 2) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (playerActorSpawnWeaponImpact(actor->weaponContacts, coord, spot) != 0) {
                        worldCoordPlaySound(spot, 0x17, 1);
                    }
                }
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 7;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (playerActorSpawnWeaponImpact(actor->weaponContacts, coord, spot) != 0) {
                worldCoordPlaySound(spot, 0x17, 1);
            }
            break;
        case 5:
        case 6:
            frames             = (u16)actor->actionValue - 1;
            actor->actionValue = frames;
            if (frames == 0) {
                if (actor->statePhase == 5) {
                    actor->statePhase++;
                    actor->actionValue                                    = 0xA;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCoordPlaySound(arg0->extra.tmd->coords, 0x201A0006, 0);
                    eff = effectSpawn(EFFECT_M4A1_BAYONET_TRAIL,
                                      actor->equipmentTasks[1]->extra.tmd->coords,
                                      0x1A, NULL);
                    if (eff != NULL) {
                        taskReparent(actor->equipmentTasks[1], eff->task);
                    }
                } else {
                    actor->statePhase                                     = 7;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
            }
            if (worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCoordPlaySound(arg0->extra.tmd->coords, 0x201A0005, 0);
            }
            break;
        case 7:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (playerActorIsSlotAdvancingLinearly(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = 0xC;
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}
