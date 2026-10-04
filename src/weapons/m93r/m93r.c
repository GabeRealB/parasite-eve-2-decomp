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

void func_m93r_8011D1C4(Task* arg0);

/// Per-frame firing state machine for the M93R burst pistol. Case 0 arms the
/// shot (four-tick reload window, `field_979` grace of 10) and queues the
/// ready animation, choosing the long variant when the weapon was left dirty
/// (`field_958`) or the actor is flagged in `field_975`; a two-handed grip
/// (`field_97F == 1`) turns the single shot into a three-round burst. Case 1
/// waits for that animation to reach its second slot. Case 2 fires one round
/// per two frames - consuming ammo 0x81, playing the muzzle report and spawning
/// the flash effect - and re-acquires the lock-on target on the off frame and
/// again once the burst runs dry. Case 3 runs out the grace counter and hands
/// back to `func_80106550`, parking `field_940` at 10 when the player is still
/// holding the fire button after the grace expired and at 0 otherwise.
void func_m93r_8011D1C4(Task* arg0)
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
    /* Declared before the switch so it is initialised in the first case test's
       delay slot, as the ROM does; case 3 reads it twice. */
    s32 lockedOut;

    SCRATCH_STACK_RESERVE_BYTES(0x50);
    spot      = SCRATCH_STACK_CURSOR(GfxCoord);
    actor     = arg0->work;
    coord     = arg0->extra.tmd->coords;
    lockedOut = 0;
    switch (actor->statePhase) {
        case 0:
            actor->state             = 4;
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->statePhase        = 1;
            actor->rumblePosted      = 0;
            actor->stateTimer        = 0;
            actor->attackCancelTicks = 0xA;
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
                    actor->stateTimer                                     = 1;
                    actor->rumblePosted                                   = 0;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    Gp_ConsumeSlotQty(0x81, 1);
                    if (func_80106264(1) == 0) {
                        actor->actionValue = 0;
                    }
                    Gp_PlayObjSfx(arg0->extra.tmd->coords, 0x20020004, 1);
                    Gp_SpawnEff(EFFECT_HANDGUN_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords, 2,
                                NULL);
                    playerActorPlayChildSlotsWithBlend(arg0, 0xA, 1, 2);
                    break;
                }
                /* Decrement through the local rather than storing `delay - 1`
                   and re-testing `delay - 1 == 0`: the latter keeps `delay`
                   live and turns the second test into a compare against the
                   switch's `1`. */
                delay--;
                actor->stateTimer = delay;
                if (delay == 0) {
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    if (Gp_PickNearestRec18(actor->weaponContacts, coord, spot) != 0) {
                        Gp_PlayObjSfx(spot, 0x17, 1);
                    }
                }
            } else {
                /* Spelled out in both arms rather than shared after the `if`;
                   GCC cross-jumps the common tail itself, keeping only the
                   `field_12A` load duplicated, which is what the ROM has. */
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
            if ((actor->padHeld & actor->actionPadMask) != 0) {
                lockedOut = actor->attackCancelTicks == 0;
            }
            if (func_80105894(arg0, D_80112E04[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0 || lockedOut) {
                if (lockedOut) {
                    actor->attackControl.cooldownTicks = 0xA;
                } else {
                    actor->attackControl.cooldownTicks = 0;
                }
                func_80106550(arg0);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x50);
}
