#include "m4a1_bayonet.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include <core/pe2.h>

#include <gameplay/display.h>
#include <gameplay/actor_render.h>
#include <gameplay/effects.h>
#include <gameplay/hud_sprites.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene.h>
#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>

#include <engine/coord.h>
#include <engine/scratch.h>
#include <engine/task_types.h>
#include <engine/mc_types.h>
#include <engine/session_types.h>
#include <engine/task.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>
#include <engine/sound_ids.h>

#include <weapons/weapon/sounds.h>
#include <weapons/weapon/weapon.h>
#include <weapons/effects/blade_trail/blade_trail.h>
#include <weapons/attack/attack.h>
#include <weapons/weapon/weapon_index.h>

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_READY_BLEND_FRAMES, int, 8)
PE2_OPTION_DEF(BURST_ROUNDS, int, 3)
PE2_OPTION_DEF(SHOT_DELAY_FRAMES, int, 3)
PE2_OPTION_DEF(IMPACT_DELAY_FRAMES, int, 2)
PE2_OPTION_DEF(BURST_CANCEL_FRAMES, int, 9)
PE2_OPTION_DEF(THRUST_CANCEL_FRAMES, int, 0xA)
PE2_OPTION_DEF(THRUST_WINDUP_FRAMES, int, 0x12)
PE2_OPTION_DEF(THRUST_CONTACT_FRAMES, int, 0xA)
PE2_OPTION_DEF(THRUST_COOLDOWN_FRAMES, int, 0x1C)
PE2_OPTION_DEF(RECOVERY_COOLDOWN_FRAMES, int, 0xC)
PE2_OPTION_DEF(THRUST_REACH, int, 0x340)
/// The thrust's ribbon trail (`m4a1BayonetTrailTask`); times in dispatch ticks.
PE2_OPTION_DEF(TRAIL_LIFETIME_FRAMES, int, 13)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
PE2_BIND_ARG(model, _symbol, gM4a1BayonetWeaponModel)

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
PE2_BIND_ARG(bank, _symbol, gM4a1BayonetPlayerAnimationBank)

/// A sound of the M4A1 Bayonet's bank, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_BAYONET, (slot))

/// The M4A1 Bayonet's own slots.
enum {
    SOUND_CONTACT = 5,
    SOUND_THRUST  = 6,
};

/// The contacts' keys and what else is wiring.
enum {
    ATTACK_KEY_BASE = 0x21A00,
    THRUST_KEY      = 0x21A1D,
};

// Defined at the end of the file, after the package's model and bank.
/// The blade's motion trail: eight tip and eight hilt coordinate frames,
/// parented to `gGfxViewCoord`. The sweep state overwrites slot
/// `EffectWork::age & 7` each frame and the ribbon is drawn between the
/// two rings.

/// Far ribbon endpoint in integer weapon-part coordinates, `(0, 0x300, 0x40)`.
///
/// The trail's persistent endpoint uses this translation relative to the held weapon.
static SVECTOR _gTrailTipOffset[1] = { { 0, 0x0300, 0x0040, 0 } };

/// Near ribbon endpoint translation, `(0, 0x180, 0x40)`, in integer coordinate units.
///
/// Recording applies it in the weapon-part frame; seeding applies the same offset
/// relative to the far endpoint, preserving the trail's initial pose.
static SVECTOR _gTrailNearOffset = { 0, 0x0180, 0x0040, 0 };

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

static GfxCoord _gBladeTrailFirstHistory[8]  = { 0 };
static GfxCoord _gBladeTrailSecondHistory[8] = { 0 };

void m4a1BayonetTrailTask(Task* task)
{
    enum {
        TRAIL_SEED   = 0,
        TRAIL_RECORD = 1,
    };
    EffectWork*    effectWork;
    GfxCoord*      tipCoord;
    GfxCoord*      historyFrame;
    GfxCoord       nearEndpoint;
    s32            effectControl;
    const SVECTOR* nearOffset;
    s32            offsetX;
    s32            offsetY;
    s32            offsetZ;
    s32            historyIndex;
    s32            keepTask;

    effectWork    = task->spawnArg2.pointer;
    tipCoord      = task->extra.coordBody->coord;
    effectControl = gRoomEffectState->effectControl;
    if (effectControl == ROOM_EFFECT_CONTROL_RUNNING) {
        effectWork->age++;
        switch (task->state) {
            case TRAIL_SEED:
                tipCoord->parent       = effectWork->parent;
                tipCoord->coord.t[0]   = _gTrailTipOffset[0].vx;
                tipCoord->coord.t[1]   = _gTrailTipOffset[0].vy;
                tipCoord->coord.t[2]   = _gTrailTipOffset[0].vz;
                tipCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(tipCoord);
                task->state = TRAIL_RECORD;

                // Only endpoint translations feed the ribbon; the temporary rotation is left uninitialized.
                // The seed chains the second endpoint beneath the tip; recording uses the weapon parent.
                // This retained one-past access reads the adjacent near-offset bytes in the image,
                // but is outside the C array's bounds. A bounded pair still needs rematching.
                offsetX                   = _gTrailTipOffset[1].vx;
                nearOffset                = &_gTrailTipOffset[1];
                offsetY                   = nearOffset->vy;
                offsetZ                   = nearOffset->vz;
                nearEndpoint.parent       = tipCoord;
                nearEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                nearEndpoint.coord.t[0]   = offsetX;
                nearEndpoint.coord.t[1]   = offsetY;
                nearEndpoint.coord.t[2]   = offsetZ;
                actorRenderComposeCoord(&nearEndpoint);

                // Seed every slot with the same world pose, detached from the moving weapon.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gBladeTrailFirstHistory); historyIndex++) {
                    historyFrame = &_gBladeTrailFirstHistory[historyIndex];
                    _bladeTrailStoreFrame(historyFrame, tipCoord);

                    historyFrame = &_gBladeTrailSecondHistory[historyIndex];
                    _bladeTrailStoreFrame(historyFrame, &nearEndpoint);
                }
                break;
            case TRAIL_RECORD:
                tipCoord->composeStamp = GRAPHICS_COORD_DIRTY;
                actorRenderComposeCoord(tipCoord);

                nearEndpoint.parent       = effectWork->parent;
                nearEndpoint.composeStamp = GRAPHICS_COORD_DIRTY;
                nearEndpoint.coord.t[0]   = _gTrailNearOffset.vx;
                nearEndpoint.coord.t[1]   = _gTrailNearOffset.vy;
                nearEndpoint.coord.t[2]   = _gTrailNearOffset.vz;
                actorRenderComposeCoord(&nearEndpoint);

                historyFrame = &_gBladeTrailFirstHistory[effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1)];
                _bladeTrailStoreFrame(historyFrame, tipCoord);

                historyFrame = &_gBladeTrailSecondHistory[effectWork->age & (ARRAY_SIZE(_gBladeTrailSecondHistory) - 1)];
                _bladeTrailStoreFrame(historyFrame, &nearEndpoint);

                // Recompose all retained world poses against the current view before drawing.
                for (historyIndex = 0; historyIndex < ARRAY_SIZE(_gBladeTrailFirstHistory); historyIndex++) {
                    historyFrame               = &_gBladeTrailFirstHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                    historyFrame               = &_gBladeTrailSecondHistory[historyIndex];
                    historyFrame->composeStamp = GRAPHICS_COORD_DIRTY;
                    actorRenderComposeCoord(historyFrame);
                }
                _bladeTrailDraw(effectWork->age & (ARRAY_SIZE(_gBladeTrailFirstHistory) - 1), BLADE_TRAIL_TINT_BLUE_WHITE);
                break;
        }
        keepTask = effectWork->age < PE2_OPTION(TRAIL_LIFETIME_FRAMES);
    } else {
        keepTask = effectControl < ROOM_EFFECT_CONTROL_CANCEL_MIN;
    }
    if (!keepTask) {
        effectKillTask(effectWork, task);
    }
}

#include <weapons/effects/blade_trail/draw.inc.c>

void m4a1BayonetAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE        = 0,
        PHASE_WAIT_READY     = 1,
        PHASE_SELECT_ATTACK  = 2,
        PHASE_BURST          = 3,
        PHASE_IMPACT         = 4,
        PHASE_THRUST_WINDUP  = 5,
        PHASE_THRUST_CONTACT = 6,
        PHASE_RECOVER        = 7,
    };
    GameActor*             actor;
    GfxCoord*              rootCoord;
    GfxCoord*              impactCoord;
    WorldCollisionCapsule* weaponCapsule;
    EffectWork*            trailWork;
    s32                    readyBlendFrames;
    s32                    shotDelay;
    s16                    thrustTicksLeft;

    actor         = playerTask->work;
    rootCoord     = playerTask->extra.tmd->coords;
    weaponCapsule = &actor->weaponShape;
    impactCoord   = SCRATCH_STACK_RESERVE_BLOCK(GfxCoord);
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            readyBlendFrames      = PE2_OPTION(READY_BLEND_FRAMES);
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase++;
            actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_CLIP_TO_GRID_CONTACT;
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
        case PHASE_SELECT_ATTACK:
            // Input selects a timed burst or the separate bayonet contact window.
            actor->rumblePosted = 0;
            if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_PRIMARY) {
                actor->statePhase                                     = PHASE_BURST;
                actor->stateTimer                                     = 0;
                actor->attackCancelTicks                              = PE2_OPTION(BURST_CANCEL_FRAMES);
                actor->actionValue                                    = PE2_OPTION(BURST_ROUNDS);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = gPlayerStatus.weaponSlotItem | ATTACK_KEY_BASE;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= WORLD_COLLISION_BODY_SINGLE_CONTACT;
                playerActorSetWeaponAttackFlags(playerTask, 0, 1);
            } else if (actor->attackButton & PLAYER_ACTOR_ATTACK_BUTTON_SECONDARY) {
                actor->statePhase        = PHASE_THRUST_WINDUP;
                actor->attackCancelTicks = PE2_OPTION(THRUST_CANCEL_FRAMES);
                actor->actionValue       = PE2_OPTION(THRUST_WINDUP_FRAMES);
                weaponRecordUse(WEAPON_INDEX_M4A1_BAYONET);
                actor->attackControl.cooldownTicks                    = PE2_OPTION(THRUST_COOLDOWN_FRAMES);
                weaponCapsule->ends[0].vz                             = weaponCapsule->ends[1].vz + PE2_OPTION(THRUST_REACH);
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].key    = THRUST_KEY;
                actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_SINGLE_CONTACT);
                playerActorSetWeaponAttackFlags(playerTask, 0, 0);
                playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_SECONDARY_FIRE, 1, 3);
                break;
            }
            /* fallthrough */
        case PHASE_BURST:
            if (actor->actionValue != 0) {
                shotDelay = actor->stateTimer;
                if (shotDelay == 0) {
                    actor->actionValue--;
                    actor->stateTimer                                     = PE2_OPTION(SHOT_DELAY_FRAMES);
                    actor->rumblePosted                                   = 0;
                    weaponCapsule->ends[0].vz                             = weaponCapsule->ends[1].vz + gPlayerActorWeaponReaches[WEAPON_INDEX_M4A1_BAYONET];
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX_M4A1_BAYONET), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
                    if (playerActorQueryWeaponLoads(PLAYER_ACTOR_WEAPON_LOAD_PRIMARY) == 0) {
                        actor->actionValue = 0;
                    }
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(WEAPON_SOUND_SLOT_SHOT), 1);
                    effectSpawn(EFFECT_RIFLE_MUZZLE_FLASH,
                                actor->equipmentTasks[1]->extra.tmd->coords,
                                WEAPON_INDEX_M4A1_BAYONET, NULL);
                    playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, 2);
                    break;
                }
                actor->stateTimer = shotDelay - 1;
                if (shotDelay - 1 == PE2_OPTION(IMPACT_DELAY_FRAMES)) {
                    _weaponResolveShotImpact(actor, rootCoord, impactCoord);
                }
                break;
            }
            /* fallthrough */
        case PHASE_IMPACT:
            actor->statePhase = PHASE_RECOVER;
            _weaponResolveShotImpact(actor, rootCoord, impactCoord);
            break;
        case PHASE_THRUST_WINDUP:
        case PHASE_THRUST_CONTACT:
            thrustTicksLeft    = (u16)actor->actionValue - 1;
            actor->actionValue = thrustTicksLeft;
            if (thrustTicksLeft == 0) {
                if (actor->statePhase == PHASE_THRUST_WINDUP) {
                    actor->statePhase++;
                    actor->actionValue                                    = PE2_OPTION(THRUST_CONTACT_FRAMES);
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags |= (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                    worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_THRUST), 0);
                    trailWork = effectSpawn(EFFECT_M4A1_BAYONET_TRAIL,
                                            actor->equipmentTasks[1]->extra.tmd->coords,
                                            WEAPON_INDEX_M4A1_BAYONET, NULL);
                    if (trailWork != NULL) {
                        taskReparent(actor->equipmentTasks[1], trailWork->task);
                    }
                } else {
                    actor->statePhase                                     = PHASE_RECOVER;
                    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                }
            }
            if (worldCollisionCountContactsByKind(actor->weaponContacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCoordPlaySound(playerTask->extra.tmd->coords, SOUND(SOUND_CONTACT), 0);
            }
            break;
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
