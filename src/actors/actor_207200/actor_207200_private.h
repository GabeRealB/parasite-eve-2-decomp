#ifndef SRC_ACTORS_ACTOR_207200_ACTOR_207200_PRIVATE_H
#define SRC_ACTORS_ACTOR_207200_ACTOR_207200_PRIVATE_H

#include "gameplay/collision.h"

/// Attack-table indices used by the Creeping Stranger's head and flank spheres.
enum {
    ACTOR_207200_CREEPING_STRANGER_ATTACK_FRONT = 0,
    ACTOR_207200_CREEPING_STRANGER_ATTACK_SIDE  = 1,
    ACTOR_207200_CREEPING_STRANGER_ATTACK_COUNT = 2,
};

/// Creeping Stranger contact damage: front 25/berserker, side 10/ordinary.
///
/// The enemy parameter record borrows both rows for the package's lifetime.
/// Contact keys copy their low twelve power bits and low four reaction bits.
extern DamageAttack gActor207200CreepingStrangerAttacks[ACTOR_207200_CREEPING_STRANGER_ATTACK_COUNT];

#endif // SRC_ACTORS_ACTOR_207200_ACTOR_207200_PRIVATE_H
