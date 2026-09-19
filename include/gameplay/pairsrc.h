#ifndef GAMEPLAY_PAIRSRC_H
#define GAMEPLAY_PAIRSRC_H

#include "common.h"

struct GpU16Pair;

/// The parameter record an enemy is built from: what it can take, what it pays
/// out, and how it reacts to being hit.
///
/// A collision body reaches it through its pair source, the pointer the enemy
/// object keeps at 0x50 (`GpObj50.field_50`, `GpEnemy.param`), which is where
/// the type's name comes from rather than from its content: `Gp_PackObjPair`
/// takes this enemy's body keys from `pairTable`, and each attack's reaction
/// parameters come from here once it has landed. The record itself carries no
/// reference back to the enemy, so several enemies of one kind share it.
///
/// The reaction flags live in the enemy object's `field_4C` and are set from the
/// kind an attack carries, by `Gp_SetObjFlag1` / `Gp_SetObjFlag2` /
/// `Gp_SetObjFlag4`; a member named `flagN…` is the parameter of the reaction
/// that flag `N` selects. Trailing pad keeps 4-byte alignment.
///
/// The overlays that hold an enemy record share this type only, so it sits in a
/// header of its own: including the pair machinery's header instead would drag
/// the gameplay prototypes into translation units that redeclare them locally
/// against their own view of a shared object.
typedef struct {
    struct GpU16Pair* pairTable;   // GpU16Pair table this enemy's body keys are packed from
    u16               hpMax;       // hit points; a body is spawned at this value
    u16               exp;         // experience awarded when the enemy is removed
    u16               bp;          // battle points awarded when the enemy is removed
    u8                mp;          // magic points awarded when the enemy is removed
    u8                critChance;  // percent base chance a hit on this enemy is rolled as a critical
    u8                flag2Ticks;  // ticks the flag-2 reaction lasts
    u8                flag4Chance; // percent chance the flag-4 reaction starts
    u8                flag4Ticks;  // ticks the flag-4 reaction lasts (0 never ends it)
    byte              pad_F;       // padding
} GpPairSrcE;
STATIC_ASSERT_SIZEOF(GpPairSrcE, 0x10);

#endif // GAMEPLAY_PAIRSRC_H
