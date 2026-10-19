#ifndef INCLUDE_ACTORS_COMPANION_H
#define INCLUDE_ACTORS_COMPANION_H

#include "types.h"

#include "main/task_types.h"

/// Native weapon IDs for the armed companion's saved resource variant.
///
/// Variants 0..4 select no weapon, P229, MM1, M950 and M4A1 Pyke respectively;
/// entry 5 is also zero, but does not establish another supported variant.
/// IDs select resident weapon collision/reach and effect profiles, independently
/// of the variant-indexed equipped model. Keep actor_800100 loaded while reading.
extern s16 gActor800100WeaponIdsByResourceVariant[6];

/// Initial weapon-attack catalog rows for saved companion resource variants 0..4.
///
/// Variants 1/3 use 9mm Spartan row 3, variant 4 uses 5.56mm row 16, and
/// variants 0/2 and the inactive sixth entry use empty row 0. These form the
/// collision key's low attack-row bits, rather than its weapon ID.
/// Keep actor_800100 loaded while reading.
extern s16 gActor800100AttackRowsByResourceVariant[6];

/// Replenished firing allowances for saved companion resource variants 0..4.
///
/// Equipment setup and reload restore 12/12/12/100/40 to the companion's
/// signed-byte-tested attack budget. Gunfire spends one per shot; this is
/// separate from the randomized attack-repeat count and Pyke flame duration.
/// Entries 5..7 are zero storage, not additional supported variants.
/// Keep actor_800100 loaded while reading.
extern u8 gActor800100AttackAllowancesByResourceVariant[8];

/// Starts the armed companion's reload behavior and stops its movement and turning.
///
/// Requires the live actor_800100 task with native animation and equipped-weapon
/// resources. `animationVariant` offsets native reload set 14 and is also stored
/// in the halfword auxiliary state; current callers pass 0. The low halfword of
/// 14 + the variant must select a loaded set, and the addition must fit s32.
/// Blends for one normal-rate frame and decays aim without clearing the target.
/// The reload handler restores the weapon's attack allowance and returns to
/// combat decisions when the clip finishes. Retains no new pointers.
void actor800100EnterReload(Task* task, s32 animationVariant);

#endif // INCLUDE_ACTORS_COMPANION_H
