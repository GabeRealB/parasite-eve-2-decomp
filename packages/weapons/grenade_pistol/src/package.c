#include "grenade_pistol.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include <core/pe2.h>

#include <gameplay/actor_render.h>
#include <gameplay/items.h>
#include <gameplay/player_actor.h>
#include <gameplay/room_effects.h>
#include <gameplay/scene_runtime.h>
#include <gameplay/world_collision.h>
#include <gameplay/world_coords.h>
#include <gameplay/player_animation.h>

#include <engine/mc.h>
#include <engine/mc_types.h>
#include <engine/session_types.h>
#include <engine/sound_ids.h>
#include <engine/task_types.h>
#include <engine/wipsys.h>
#include <engine/wipsys_types.h>

#include <projectiles/launcher_grenade/launcher_grenade.h>

#include <weapons/weapon/sounds.h>
#include <weapons/weapon/weapon.h>
#include <weapons/weapon/weapon_index.h>

/// Which launcher it is: 0 the Grenade Pistol, 1 the MM1. It is the row of the grenade's launch tuning, and the weapon's index less 11.
PE2_OPTION_DEF(LAUNCHER, int)

PE2_EDITOR_LABEL("Attack")
/// Attack tuning, in dispatch ticks.
PE2_OPTION_DEF(READY_BLEND_FRAMES, int, 1)
PE2_OPTION_DEF(MOVING_BLEND_FRAMES, int, 8)
PE2_OPTION_DEF(FIRE_BLEND_FRAMES, int, 3)
PE2_OPTION_DEF(SHOT_COOLDOWN_FRAMES, int, 40)

PE2_EDITOR_LABEL("Model")
/// The model held in the player's weapon hand.
PE2_BIND(model, model)
#if PE2_OPTION(LAUNCHER) == 1
PE2_BIND_ARG(model, _symbol, gMm1WeaponModel)
#else
PE2_BIND_ARG(model, _symbol, gGrenadePistolWeaponModel)
#endif

PE2_EDITOR_LABEL("Animation")
/// The player's clips while it is equipped, by slot.
PE2_BIND(bank, animation_bank)
#if PE2_OPTION(LAUNCHER) == 1
PE2_BIND_ARG(bank, _symbol, gMm1PlayerAnimationBank)
#else
PE2_BIND_ARG(bank, _symbol, gGrenadePistolPlayerAnimationBank)
#endif

/// The launched grenade's model.
PE2_BIND(grenade_model, model)
#if PE2_OPTION(LAUNCHER) == 1
PE2_BIND_ARG(grenade_model, _symbol, gMm1ShellModel)
#else
PE2_BIND_ARG(grenade_model, _symbol, gGrenadePistolShellModel)
#endif

/// A sound of the bank of the launcher being built, by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define SOUND(slot) SOUND_WEAPON(WEAPON_INDEX, (slot))

enum {
    SOUND_LAUNCH = 4, // The bank's shot slot (the detonation is public)
};

static void _launcherGrenadeExit(Task* task);
static void _launcherGrenadeSpawn(Task* task);

/// One-based weapon index for this launcher build: 11 Grenade Pistol or 12 MM1.
///
/// Parenthesized int constant expression from the validated build selector.
/// Selects the sound bank, muzzle-flash profile and projectile distance row;
/// `WEAPON_ITEM_ID` converts it to the saved weapon-load item ID.
#define WEAPON_INDEX (WEAPON_INDEX_GRENADE_PISTOL + (PE2_OPTION(LAUNCHER)))

/// The model and the player's clips: here, as the package stores them after the data above.
PE2_VALUE(model)
PE2_VALUE(bank)

/// Grenade Pistol/MM1 muzzle offsets in integer launcher-local coordinates.
///
/// Rows 0 and 1 select the Grenade Pistol and MM1 respectively, from bits 16..19
/// of the shot's spawn argument. Kyle's MM1 uses row 1. Only XYZ is consumed.
static SVECTOR _gLauncherGrenadeMuzzleOffsets[2] = {
    { 0, 480, 128, 0 },
    { 0, 544, 128, 0 },
};

/// Blast-sphere radii in game-coordinate units for the three grenade rounds.
///
/// Indexed by the loaded round minus `GRENADE_ROUND_FIRST`: fragmentation 500,
/// airburst 1200 and riot 2000. The fourth stored halfword is zero; its role is
/// unproven and no supported round selects it.
static u16 _gLauncherGrenadeBlastRadii[4] = {
    [GRENADE_ROUND_FRAGMENTATION - GRENADE_ROUND_FIRST] = 500,
    [GRENADE_ROUND_AIRBURST -
        GRENADE_ROUND_FIRST]                            = 1200,
    [GRENADE_ROUND_RIOT -
        GRENADE_ROUND_FIRST]                            = 2000,
    0,
};

/// Initial positive flight divisors for launcher rows 0 Grenade Pistol and 1 MM1.
///
/// The byte value is shifted left 16 into the Q16.16 flight clock. Flight divides
/// the Q12 direction by its integer half, then increases the divisor each tick.
/// The two trailing stored zero bytes have unproven roles and are never selected
/// by supported launchers; they are not valid initial divisors.
static u8 _gLauncherGrenadeInitialFlightDivisors[4] = { 12, 8, 0, 0 };

/// The launched grenade's model.
PE2_VALUE(grenade_model)

void grenadePistolAttackState(Task* playerTask)
{
    enum {
        PHASE_PREPARE    = 0,
        PHASE_WAIT_READY = 1,
        PHASE_FIRE       = 2,
        PHASE_RECOVER    = 3,
    };
    GameActor* actor;
    s32        blendFrames;

    actor = playerTask->work;
    switch (actor->statePhase) {
        case PHASE_PREPARE:
            blendFrames           = PE2_OPTION(READY_BLEND_FRAMES);
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase    += blendFrames;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                blendFrames = PE2_OPTION(MOVING_BLEND_FRAMES);
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, blendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case PHASE_FIRE:
            actor->statePhase                  = PHASE_RECOVER;
            actor->rumblePosted                = 0;
            actor->attackControl.cooldownTicks = PE2_OPTION(SHOT_COOLDOWN_FRAMES);
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - GRENADE_ROUND_FIRST) | SOUND(SOUND_LAUNCH), 1);
            effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords, WEAPON_INDEX,
                        NULL);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            // Both the projectile package and its launch-tuning row follow this build.
            playerActorSpawnGrenadeProjectile(playerTask, PLAYER_ACTOR_GRENADE_PLAYER, PLAYER_ACTOR_GRENADE_PISTOL + PE2_OPTION(LAUNCHER),
                                              gPlayerStatus.weaponSlotItem | (PE2_OPTION(LAUNCHER) << PLAYER_ACTOR_GRENADE_MUZZLE_ROW_SHIFT) | (WEAPON_INDEX << PLAYER_ACTOR_GRENADE_WEAPON_SHIFT));
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, PE2_OPTION(FIRE_BLEND_FRAMES));
            break;
        case PHASE_RECOVER:
            // `_weaponIsArmSlotAdvancing` (weapons/attack.h) spelled out: as the inline, the test compiles differently here.
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
}

#include <projectiles/launcher_grenade/launcher_grenade_flight.inc.c>

#include <projectiles/launcher_grenade/launcher_grenade.inc.c>

/// Projectile state handlers for the Grenade Pistol and MM1 builds.
///
/// `Task::state` selects 0 spawn, 1 flight, 2 blast countdown or 3 teardown.
/// The dispatcher copies all four `void(Task*)` callbacks before calling one;
/// there is no terminator or bounds check. Keep the matching weapon package
/// loaded through dispatch. Spawn allocates work; later states require it live,
/// and teardown releases the task after unlinking its collision bodies.
static const TaskFuncTable4 _gShellStateHandlers = {
    .funcs = {
        _launcherGrenadeSpawn,
        _launcherGrenadeFly,
        _launcherGrenadeBlast,
        _launcherGrenadeExit,
    },
};

void grenadePistolShellTask(Task* task)
{
    TaskFuncTable4 stateHandlers;

    stateHandlers = _gShellStateHandlers;
    stateHandlers.funcs[task->state](task);
}
