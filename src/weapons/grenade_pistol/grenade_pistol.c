#include "weapons/grenade_pistol.h"

#include "config.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "gameplay/actor_render.h"
#include "gameplay/items.h"
#include "gameplay/player_actor.h"
#include "gameplay/room_effects.h"
#include "gameplay/scene_runtime.h"
#include "gameplay/world_collision.h"
#include "gameplay/world_coords.h"

#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session_types.h"
#include "main/sound_ids.h"
#include "main/task_types.h"
#include "main/wipsys.h"
#include "main/wipsys_types.h"

#include "weapons/sounds.h"
#include "weapons/weapon.h"
#include "../../shared/projectiles/launcher_grenade.h"

#include "assets/engine/animation/player_schema.h"

static void _launcherGrenadeExit(Task* task);
static void _launcherGrenadeSpawn(Task* task);

/// Grenade Pistol discriminator for `GRENADE_PISTOL_VARIANT` and launch-tuning row 0.
///
/// Integer preprocessor constant; selects this package's models and animation bank.
#define GRENADE_PISTOL_VARIANT_PISTOL 0

/// MM1 discriminator for `GRENADE_PISTOL_VARIANT` and launch-tuning row 1.
///
/// Integer preprocessor constant; selects the MM1 models and animation bank.
#define GRENADE_PISTOL_VARIANT_MM1 1

/// Manifest-selected launcher build and projectile launch-tuning row.
///
/// Required integer preprocessor expression: 0 Grenade Pistol or 1 MM1.
/// Selects held/shell models and the player animation bank, derives weapon
/// index 11/12 and projectile launcher index 1/2, and occupies spawn bits 16..19
/// to select muzzle offsets and the initial flight divisor. Both packages
/// retain both tuning rows for the shared projectile implementation.
#ifndef GRENADE_PISTOL_VARIANT
#error "GRENADE_PISTOL_VARIANT is a per-package build parameter"
#elif GRENADE_PISTOL_VARIANT != GRENADE_PISTOL_VARIANT_PISTOL && GRENADE_PISTOL_VARIANT != GRENADE_PISTOL_VARIANT_MM1
#error "GRENADE_PISTOL_VARIANT must select a Grenade Pistol or MM1 build"
#endif

/// One-based weapon index for this launcher build: 11 Grenade Pistol or 12 MM1.
///
/// Parenthesized int constant expression from the validated build selector.
/// Selects the sound bank, muzzle-flash profile and projectile distance row;
/// `WEAPON_ITEM_ID` converts it to the saved weapon-load item ID.
#define GRENADE_PISTOL_WEAPON_INDEX (WEAPON_INDEX_GRENADE_PISTOL + (GRENADE_PISTOL_VARIANT))

/* Each package carries its own model. */
#if GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_PISTOL
/// The model `gGrenadePistolWeaponModel`: generated from its asset (weapons/grenade_pistol/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/grenade_pistol/model.inc"
#elif GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_MM1
/// The model `gMm1WeaponModel`: generated from its asset (weapons/mm1/model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/mm1/model.inc"
#endif

/* Each package carries its own animation bank. */
#if GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_PISTOL
/// The player's clips this package carries, in the package's order, and its animation bank (weapons/grenade_pistol/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/grenade_pistol/clips.inc"
#include "assets/weapons/grenade_pistol/bank.inc"
#elif GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_MM1
/// The player's clips this package carries, in the package's order, and its animation bank (weapons/mm1/bank):
/// generated from the clip and bank assets by tools/buildsteps/gen_animation_source.py.
#include "assets/weapons/mm1/clips.inc"
#include "assets/weapons/mm1/bank.inc"
#endif

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

/* Each package carries its own shell model. */
#if GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_PISTOL
/// The model `gGrenadePistolShellModel`: generated from its asset (weapons/grenade_pistol/grenade_model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/grenade_pistol/grenade_model.inc"
#elif GRENADE_PISTOL_VARIANT == GRENADE_PISTOL_VARIANT_MM1
/// The model `gMm1ShellModel`: generated from its asset (weapons/mm1/grenade_model) by tools/buildsteps/gen_model_source.py.
#include "assets/weapons/mm1/grenade_model.inc"
#endif

void grenadePistolAttackState(Task* playerTask)
{
    enum {
        GRENADE_PISTOL_PHASE_PREPARE    = 0,
        GRENADE_PISTOL_PHASE_WAIT_READY = 1,
        GRENADE_PISTOL_PHASE_FIRE       = 2,
        GRENADE_PISTOL_PHASE_RECOVER    = 3,
    };
    GameActor* actor;
    s32        blendFrames;

    actor = playerTask->work;
    switch (actor->statePhase) {
        case GRENADE_PISTOL_PHASE_PREPARE:
            blendFrames           = GRENADE_PISTOL_READY_BLEND_FRAMES;
            actor->state          = PLAYER_ACTOR_STATE_ATTACK;
            actor->mode           = GAME_ACTOR_MODE_NORMAL;
            actor->turnRateIndex  = 0;
            actor->animationState = 0;
            actor->statePhase    += blendFrames;
            if (((u16)actor->movementMode | actor->turnSign) != 0) {
                blendFrames = GRENADE_PISTOL_MOVING_BLEND_FRAMES;
            }
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_AIM_IDLE, 0, blendFrames);
            actor->movementMode = GAME_ACTOR_MOVEMENT_STOPPED;
            break;
        case GRENADE_PISTOL_PHASE_WAIT_READY:
            if (animationGetCurrentRecord(&actor->animationContext, actor->animationSlots + 1) !=
                NULL) {
                actor->statePhase++;
            }
            break;
        case GRENADE_PISTOL_PHASE_FIRE:
            actor->statePhase                  = GRENADE_PISTOL_PHASE_RECOVER;
            actor->rumblePosted                = 0;
            actor->attackControl.cooldownTicks = GRENADE_PISTOL_SHOT_COOLDOWN_TICKS;
            worldCoordPlaySound(playerTask->extra.tmd->coords,
                                WEAPON_SOUND_VARIANT(gPlayerStatus.weaponSlotItem - GRENADE_ROUND_FIRST) | GRENADE_PISTOL_SOUND(GRENADE_PISTOL_SOUND_LAUNCH), 1);
            effectSpawn(EFFECT_GRENADE_MUZZLE_FLASH,
                        actor->equipmentTasks[1]->extra.tmd->coords, GRENADE_PISTOL_WEAPON_INDEX,
                        NULL);
            equipmentConsumeWeaponLoad(WEAPON_ITEM_ID(GRENADE_PISTOL_WEAPON_INDEX), EQUIPMENT_WEAPON_LOAD_CONSUME_PRIMARY);
            // Both the projectile package and its launch-tuning row follow this build.
            playerActorSpawnGrenadeProjectile(playerTask, PLAYER_ACTOR_GRENADE_PLAYER, PLAYER_ACTOR_GRENADE_PISTOL + GRENADE_PISTOL_VARIANT,
                                              gPlayerStatus.weaponSlotItem | (GRENADE_PISTOL_VARIANT << PLAYER_ACTOR_GRENADE_MUZZLE_ROW_SHIFT) | (GRENADE_PISTOL_WEAPON_INDEX << PLAYER_ACTOR_GRENADE_WEAPON_SHIFT));
            playerActorPlayChildSlotsWithBlend(playerTask, PLAYER_ANIMATION_FIRE, 0, GRENADE_PISTOL_FIRE_BLEND_FRAMES);
            break;
        case GRENADE_PISTOL_PHASE_RECOVER:
            // `_weaponIsArmSlotAdvancing` (weapons/shared/attack.h) spelled out: as the inline, the test compiles differently here.
            if (playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0) == 0) {
                playerActorFinishWeaponAttack(playerTask);
            }
            break;
    }
}

#include "../../shared/projectiles/launcher_grenade_flight.inc.c"

#include "../../shared/projectiles/launcher_grenade.inc.c"

/// Projectile state handlers for the Grenade Pistol and MM1 builds.
///
/// `Task::state` selects 0 spawn, 1 flight, 2 blast countdown or 3 teardown.
/// The dispatcher copies all four `void(Task*)` callbacks before calling one;
/// there is no terminator or bounds check. Keep the matching weapon package
/// loaded through dispatch. Spawn allocates work; later states require it live,
/// and teardown releases the task after unlinking its collision bodies.
static const TaskFuncTable4 _gGrenadePistolShellStateHandlers = {
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

    stateHandlers = _gGrenadePistolShellStateHandlers;
    stateHandlers.funcs[task->state](task);
}

PACKAGE_ALIASES
