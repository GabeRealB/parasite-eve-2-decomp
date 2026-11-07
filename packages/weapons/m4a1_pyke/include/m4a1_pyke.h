#ifndef WEAPONS_M4A1_PYKE_H
#define WEAPONS_M4A1_PYKE_H

#include <gameplay/animation.h>

#include <engine/sound_ids.h>
#include <engine/task_types.h>
#include <engine/tmd_types.h>

#include <weapons/weapon/weapon_index.h>

/// A sound of the M4A1 Pyke's bank (weapons/m4a1_pyke/sounds), by slot (`WEAPON_SOUND_SLOT_*`, or one below).
#define M4A1_PYKE_SOUND(slot) SOUND_WEAPON(WEAPON_INDEX_M4A1_PYKE, (slot))

enum {
    /// The flamethrower: started with the secondary attack, stopped when it ends; gameplay also stops it when the
    /// attack is interrupted.
    M4A1_PYKE_SOUND_FLAME = 5,
};

/// The player's clips while the M4A1 Pyke is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 28.
extern AnimationSet* gM4a1PykePlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The M4A1 with its Pyke flamethrower attachment, held in the player's weapon hand.
extern TmdSource gM4a1PykeWeaponModel;

/// Requests for the nozzle, stored in `Task::spawnArg1.value`.
enum {
    M4A1_PYKE_NOZZLE_OFF        = 0,
    M4A1_PYKE_NOZZLE_IDLE       = 1,
    M4A1_PYKE_NOZZLE_FIRE       = 2,
    M4A1_PYKE_NOZZLE_RESET_IDLE = 3,
    M4A1_PYKE_NOZZLE_RESET_OFF  = 4,
    M4A1_PYKE_NOZZLE_RELEASE    = 5,
};

/// The Pyke's nozzle, the player's persistent weapon effect (`GameActor::weaponEffectTask`): requested by
/// `M4A1_PYKE_NOZZLE_*`, it draws a pilot flame and its light when idle, and when firing spews a flame every frame,
/// faster and faster.
void m4a1PykeNozzleTask(Task* task);

/// A flame from the Pyke's nozzle, launched at the speed in `spawnArg1`: it ends on the first enemy it touches, spreads
/// out where it hits a wall, and otherwise dies away after a short flight.
void m4a1PykeFlameTask(Task* task);

/// The M4A1 Pyke's attack: a three-round burst on the primary button, stopping when the magazine runs out, and flame
/// on the secondary for as long as it is held and fuel lasts; a held action input ends its recovery early.
void m4a1PykeAttackState(Task* playerTask);

#endif // WEAPONS_M4A1_PYKE_H
