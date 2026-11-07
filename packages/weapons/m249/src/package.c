#include "m249.h"

#include <core/pe2.h>

#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_coords.h>

#include <engine/coord.h>
#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session_types.h>
#include <engine/task_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/weapon_index.h>

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
} _AttackScratch;
STATIC_ASSERT_SIZEOF(_AttackScratch, 0x68);

#include <weapons/weapon/weapon.h>
#include <weapons/weapon/sounds.h>

#include <weapons/attack/attack.h>

#include <gameplay/player_animation.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 8)
PE2_OPTION_DEF(SHOT_DELAY_FRAMES, int, 2)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gM249WeaponModel)
PE2_VALUE(model)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gM249PlayerAnimationBank)
PE2_VALUE(bank)

/// A sound of the M249's bank, by slot (`WEAPON_SOUND_SLOT_*`).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M249, (slot))

void m249AttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE    = 0,
        PHASE_WAIT_READY = 1,
        PHASE_ARM_SHOT   = 2,
        PHASE_SHOT_DELAY = 3,
        PHASE_IMPACT     = 4,
        PHASE_RECOVER    = 5,
    };
    GameActor*      actor;
    GfxCoord*       rootCoord;
    GfxCoord*       impactCoord;
    _AttackScratch* scratch;
    s32             readyBlendFrames;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(_AttackScratch);
    actor     = playerTask->work;
    rootCoord = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            readyBlendFrames      = PE2_OPTION(READY_BLEND_FRAMES);
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->turnRateIndex  = 2;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT);
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                readyBlendFrames = PE2_OPTION(MOVING_READY_BLEND_FRAMES);
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, readyBlendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_ARM_SHOT:
        fire:
            actor->statePhase                  = PHASE_SHOT_DELAY;
            actor->attackControl.cooldownTicks = 0;
            actor->rumblePosted                = 0;
            actor->stateTimer                  = PE2_OPTION(SHOT_DELAY_FRAMES);
            playerActorSetWeaponAttackFlags(playerTask, 0, actor->attackButton != PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY);
            /* fallthrough */
        case PHASE_SHOT_DELAY:
            // The arm dispatch consumes the first of the two delay ticks.
            if (--actor->stateTimer == 0) {
                actor->statePhase++;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M249), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                            actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX_M249,
                            NULL);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 1, PE2_OPTION(SHOT_DELAY_FRAMES));
            }
            break;
        case PHASE_IMPACT:
            impactCoord         = &scratch->impactCoord;
            actor->movementSign = 0;
            actor->statePhase++;
            _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            /* fallthrough */
        case PHASE_RECOVER:
            if (playerActorReadAttackButton(playerTask) != 0 && playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) > 0) {
                goto fire;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    playerActorTrackLockTarget(playerTask);
    SCRATCH_STACK_RELEASE_BLOCK(_AttackScratch);
}
