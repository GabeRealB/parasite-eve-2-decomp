#ifndef WEAPONS_GUNBLADE_H
#define WEAPONS_GUNBLADE_H

#include <gameplay/animation.h>

#include <engine/task_types.h>

/// The player's clips while the Gunblade is equipped, by slot (`PLAYER_ANIMATION_*`): gameplay's bank for weapon 23.
extern AnimationSet* gGunbladePlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

/// The ribbon behind a Gunblade slash, blue-white then yellow-white as it ages; a round fired through the slash
/// spawns the charge flash from it, graded by the round (`spawnArg1`).
void gunbladeTrailTask(Task* task);

/// The flash of a round fired through a Gunblade slash: a disc, bands and a screen tint that grow and fade, with
/// flashes, bands and sparks, colored by the round in `spawnArg1` (Buckshot, Firefly or R. Slug).
void gunbladeChargeFlashTask(Task* task);

/// The Gunblade's attack: on the primary button a spinning slash that carries the player forward, where the secondary
/// button in its contact window fires a round through it; on the secondary a shot of the loaded shell.
void gunbladeAttackState(Task* playerTask);

#endif // WEAPONS_GUNBLADE_H
