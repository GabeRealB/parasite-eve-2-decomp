#include "weapons/m249.h"

#include "common.h"

#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_coords.h"

#include "main/coord.h"
#include "main/mc.h"
#include "main/mc_types.h"
#include "main/scratch.h"
#include "main/sound.h"
#include "main/session_types.h"
#include "main/task_types.h"

/// Scratch-stack block for the M249's attack handler.
///
/// The handler reserves one block each frame and releases it before
/// returning; the block is not cleared. It stages the node a fired round's
/// impact sound is placed at.
///
/// Only the translation of `impactCoord`'s composed transform is written, and
/// only when the impact picker reports a hit, which is the one case the node
/// is then read in. A sound's pan and depth come from projecting the node's
/// origin, to which its rotation contributes nothing, so the rest of the node
/// is whatever the scratch stack last held. The position is a weapon contact
/// point offset by up to 7 units per axis, in the space the weapon node's
/// composed transform is expressed in.
///
/// The 0x18 bytes ahead of the node are reserved with it and never accessed.
typedef struct {
    byte     field_0[0x18]; // No recovered access; role unproven
    GfxCoord impactCoord;   // Node standing at the round's impact point: the source of the impact sound
} _M249AttackScratch;
STATIC_ASSERT_SIZEOF(_M249AttackScratch, 0x68);

#include "weapons/weapon.h"

enum { M249_IMPACT_SOUND = SOUND_COMMON(0x17) };

/// Ends M249 shot contact tests and requests the chosen surface's impact effect and sound.
///
/// Clears only the weapon body's grid/pair enables; keeps it linked and its
/// complete six-entry contact table intact. Requires live player/room state,
/// initialized contacts and `rootCoord->workm` in the contacts' view frame.
/// Enemy-body hits suppress surface selection. A selected eligible grid point
/// receives 0..7 units of XYZ jitter; only `impactCoord`'s three cached
/// translation words are written. Failure leaves that temporary node untouched.
/// On success, requests the common impact sound synchronously and latches
/// combat noise. Scratch/GTE requirements follow the picker and sound APIs;
/// this helper neither releases nor retains the caller's writable impact node.
static inline void _m249ResolveShotImpact(GameActor* actor, const GfxCoord* rootCoord, GfxCoord* impactCoord)
{
    enum { M249_IMPACT_SIGNAL_NOISE = 1 };

    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if (playerActorSpawnWeaponImpact(actor->weaponContacts, rootCoord, impactCoord) != 0) {
        worldCoordPlaySound(impactCoord, M249_IMPACT_SOUND, M249_IMPACT_SIGNAL_NOISE);
    }
}

void m249AttackState(Task* playerTask)
{
    enum {
        M249_PHASE_PREPARE             = 0,
        M249_PHASE_WAIT_READY          = 1,
        M249_PHASE_ARM_SHOT            = 2,
        M249_PHASE_SHOT_DELAY          = 3,
        M249_PHASE_IMPACT              = 4,
        M249_PHASE_RECOVER             = 5,
        M249_PLAYER_ATTACK_STATE       = 4,
        M249_ANIMATION_READY           = 9,
        M249_ANIMATION_PRIMARY         = 0xA,
        M249_READY_BLEND_FRAMES        = 1,
        M249_WEAPON_ID                 = 17,
        M249_MOVING_READY_BLEND_FRAMES = 8,
        M249_SHOT_DELAY_FRAMES         = 2,
        M249_FIRE_SOUND                = SOUND_WEAPON(17, 4),
    };
    GameActor*          actor;
    GfxCoord*           rootCoord;
    GfxCoord*           impactCoord;
    _M249AttackScratch* scratch;
    s32                 readyBlendFrames;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_M249AttackScratch);
    actor     = playerTask->work;
    rootCoord = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case M249_PHASE_PREPARE:
            readyBlendFrames      = M249_READY_BLEND_FRAMES;
            actor->state          = M249_PLAYER_ATTACK_STATE;
            actor->turnRateIndex  = 2;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = M249_MOVING_READY_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, M249_ANIMATION_READY, 0, readyBlendFrames);
            actor->movementMode = 0;
            break;
        case M249_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case M249_PHASE_ARM_SHOT:
        fire:
            actor->statePhase                  = M249_PHASE_SHOT_DELAY;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = M249_SHOT_DELAY_FRAMES;
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton != PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            /* fallthrough */
        case M249_PHASE_SHOT_DELAY:
            // The arm dispatch consumes the first of the two delay ticks.
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(M249_WEAPON_ID), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, M249_FIRE_SOUND, 1);
                effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, M249_WEAPON_ID,
                            NULL);
                playerActorPlayChildSlotsWithBlend(playerTask, M249_ANIMATION_PRIMARY, 1, M249_SHOT_DELAY_FRAMES);
            }
            break;
        case M249_PHASE_IMPACT:
            impactCoord         = &scratch->impactCoord;
            actor->movementSign = 0;
            actor->statePhase++;
            _m249ResolveShotImpact(actor, rootCoord, impactCoord);
            /* fallthrough */
        case M249_PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0) {
                goto fire;
            }
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(_M249AttackScratch);
}

/// The model `gM249WeaponModel`: generated from its asset (weapons/m249/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/m249/model.inc"

/// The player's clips this package carries, in the package's order, and its animation bank (weapons/m249/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/m249/clips.inc"
#include "assets/weapons/m249/bank.inc"
