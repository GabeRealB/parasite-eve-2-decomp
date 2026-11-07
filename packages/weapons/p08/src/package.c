#include "p08.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include <core/pe2.h>

#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>

#include <engine/coord.h>
#include <engine/mc_types.h>
#include <engine/scratch.h>
#include <engine/session_types.h>
#include <engine/task_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/weapon.h>
#include <weapons/weapon/sounds.h>
#include <weapons/attack/attack.h>
#include <weapons/weapon/weapon_index.h>

/// The weapon the package is (its index: the item it is, less 0x7F): 4 the P08, 1 its snail-magazine build, 9 the Mongoose.
PE2_OPTION_DEF(WEAPON_INDEX, int)

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 5)
PE2_OPTION_DEF(CANCEL_FRAMES, int, 0xB)

/// Ticks after recovery ends before another attack.
PE2_OPTION_DEF(RECOVERY_COOLDOWN_FRAMES, int, 12)
STATIC_ASSERT(PE2_OPTION(RECOVERY_COOLDOWN_FRAMES) >= 0 && PE2_OPTION(RECOVERY_COOLDOWN_FRAMES) <= 32767, cooldown_is_a_nonnegative_s16);
/// Its muzzle's flash: a handgun's (both P08s), or the Mongoose's, an actor's flash at its own muzzle offsets.
PE2_OPTION_DEF(FLASH, enum, handgun, mongoose)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
#if PE2_OPTION_IS(FLASH, mongoose)
PE2_BIND_ARG(model, _symbol, gMongooseWeaponModel)
#else
PE2_BIND_ARG(model, _symbol, gP08WeaponModel)
#endif
PE2_VALUE(model)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
#if PE2_OPTION_IS(FLASH, mongoose)
PE2_BIND_ARG(bank, _symbol, gMongoosePlayerAnimationBank)
#else
PE2_BIND_ARG(bank, _symbol, gP08PlayerAnimationBank)
#endif
PE2_VALUE(bank)

/// The muzzle profiles the flash reads: both P08s share the handgun's (1), though the P08's weapon index is 4.
enum {
    FLASH_PROFILE_HANDGUN  = 1,
    FLASH_PROFILE_MONGOOSE = 9,
};

/// What the flash is: the effect spawned, and the muzzle profile it reads.
#if PE2_OPTION_IS(FLASH, mongoose)
#define MUZZLE_FLASH_EFFECT  EFFECT_ACTOR_MUZZLE_FLASH
#define MUZZLE_FLASH_PROFILE FLASH_PROFILE_MONGOOSE
#else
#define MUZZLE_FLASH_EFFECT  EFFECT_HANDGUN_MUZZLE_FLASH
#define MUZZLE_FLASH_PROFILE FLASH_PROFILE_HANDGUN
#endif

/// A sound of the bank of the weapon being built, by slot (`WEAPON_SOUND_SLOT_*`).
#define SOUND(slot) SOUND_WEAPON(PE2_OPTION(WEAPON_INDEX), (slot))

void p08AttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE    = 0,
        PHASE_WAIT_READY = 1,
        PHASE_FIRE       = 2,
        PHASE_IMPACT     = 3,
        PHASE_RECOVER    = 4,
    };
    GameActor* actor;
    GfxCoord*  rootCoord;
    GfxCoord*  impactCoord;
    s32        readyBlendFrames;

    impactCoord = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    actor       = playerTask->work;
    rootCoord   = playerTask->extra.tmd->coords;
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            actor->state             = PLAYER_ACTOR_STATE_ATTACK;
            actor->statePhase        = PHASE_WAIT_READY;
            readyBlendFrames         = PE2_OPTION(READY_BLEND_FRAMES);
            actor->mode              = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex     = 0;
            actor->animationState    = 0;
            actor->rumblePosted      = 0;
            actor->attackCancelTicks = PE2_OPTION(CANCEL_FRAMES);
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
        case PHASE_FIRE:
            actor->statePhase++;
            playerActorSetWeaponAttackFlags(playerTask, 0, 0);
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT | WORLD_COLLISION_BODY_SINGLE_CONTACT | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(PE2_OPTION(WEAPON_INDEX)), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
            effectSpawn(MUZZLE_FLASH_EFFECT,
                        actor->equipmentTasks[1]->extra.tmd->coords,
                        MUZZLE_FLASH_PROFILE, NULL);
            playerActorResetChildSlots(playerTask, PLAYER_ANIMATION_FIRE);
            break;
        case PHASE_IMPACT:
            actor->statePhase++;
            _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            /* fallthrough */
        case PHASE_RECOVER:
            if (actor->attackCancelTicks != 0) {
                actor->attackCancelTicks--;
            }
            if (_weaponIsArmSlotAdvancing(playerTask) == 0 ||
                ((actor->padHeld & actor->actionPadMask) != 0 && actor->attackCancelTicks == 0)) {
                actor->attackControl.cooldownTicks = PE2_OPTION(RECOVERY_COOLDOWN_FRAMES);
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GfxCoord);
}
