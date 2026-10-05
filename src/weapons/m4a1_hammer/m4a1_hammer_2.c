#include <psyq/sys/types.h>
#include <psyq/libgte.h>

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
#include "main/wipsys.h"
#include "main/wipsys_types.h"

void func_m4a1_hammer_8011E710(Task* arg0);

/// Per-frame firing state machine for the M4A1 hammer. State 0 arms the shot
/// and raises the weapon (clip 8 instead of 1 when it was already up), state 1
/// waits for that clip. State 2 branches on `field_97F`: a held trigger (bit 0)
/// drops into the three-round burst of state 3, a tap (bit 1) swings the hammer
/// (state 5) after telling the hammer task (`field_914`) to go to sub-state 2,
/// and anything else falls straight into the burst. Both branches re-aim the
/// muzzle record at `field_14C`, the hammer with a 0x400 spread and the burst
/// with 0x100 plus the attach-0x19 row of `D_80112F60`. State 3 counts
/// `field_934` down to each round, spending one magazine round, playing
/// `0x20190004` and spawning the muzzle flash, and picks the lock-on target on
/// the frame after. State 4 picks that target once and hands over to state 6.
/// State 5 counts the swing out over `field_934`, flagging the wind-up at 1 and
/// releasing the hammer task at 0. State 6 counts `field_979` down and drops
/// out of the firing pose once the aim check fails or the trigger has been
/// released.
void func_m4a1_hammer_8011E710(Task* arg0)
{
    GameActor*             actor;
    GfxCoord*              coord;
    GfxCoord*              spot;
    WorldCollisionCapsule* rec;
    Task*                  hammer;
    s32                    anim;
    s32                    delay;
    u16                    flags;

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
                actor->turnRateIndex                                  = 0;
                actor->stateTimer                                     = 0;
                actor->attackCancelTicks                              = 9;
                actor->actionValue                                    = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | 0x21900;
                rec->end0Radius                                       = 0x100;
                rec->end1Radius                                       = 0x100;
                rec->ends[0].vz                                       = rec->ends[1].vz + D_80112F60[0x19];
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= 0x800;
                func_80106238(arg0, 0, 1);
            } else if (actor->attackButton & 2) {
                actor->statePhase                                     = 5;
                actor->turnRateIndex                                  = 2;
                actor->attackCancelTicks                              = 0x1C;
                actor->actionValue                                    = 0x14;
                actor->stateTimer                                     = 3;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = 0x2191C;
                rec->end0Radius                                       = 0x400;
                rec->end1Radius                                       = 0x400;
                rec->ends[0].vz                                       = rec->ends[1].vz + 0xA00;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= 0xF7FF;
                func_80106238(arg0, 0, 0);
                hammer = actor->weaponEffectTask;
                if (hammer != NULL) {
                    hammer->spawnArg1.value = 2;
                }
                Gp_ConsumeSlotQty(0x98, 0x101);
                Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20190005, 1);
                playerActorPlayChildSlotsWithBlend(arg0, 0xB, 0, 2);
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
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(0x98, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20190004, 1);
                    Gp_SpawnEff(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                0x19, NULL);
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
                break;
            }
            /* fallthrough */
        case 4:
            actor->statePhase                                     = 6;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                Gp_PlayObjSfx(spot, 0x17, 1);
            }
            break;
        case 5:
            delay             = actor->stateTimer - 1;
            actor->stateTimer = delay;
            if (delay == 1) {
                flags                                                = actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = flags;
            } else if (delay == 0) {
                actor->statePhase = 6;
                if (func_80106264(2) == 0) {
                    actor->weaponEffectTask->spawnArg1.value = 0;
                }
                flags                                                = actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags = flags;
            }
            /* fallthrough */
        case 6:
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
