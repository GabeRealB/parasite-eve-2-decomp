/* Helpers the weapons' attack states share: the shot impact of the guns whose
 * rounds end in the common bullet impact (the M93R, M950, P08, M249 and the M4A1
 * with its Bayonet, Hammer and Pyke attachments), and the test that ends an
 * attack's recovery.
 *
 * Include this header in the prologue.
 */

#ifndef SRC_WEAPONS_SHARED_ATTACK_H
#define SRC_WEAPONS_SHARED_ATTACK_H

#include "gameplay/player_actor.h"
#include "gameplay/world_collision_types.h"
#include "gameplay/world_coords.h"

#include "main/mc.h"
#include "main/mc_types.h"
#include "main/session_types.h"
#include "main/sound_ids.h"

/// Ends a shot's contact tests and requests the chosen surface's impact effect and sound.
///
/// Clears only the weapon body's grid/pair enables; keeps it linked and its
/// complete six-entry contact table intact. Requires live player/room state,
/// initialized contacts and `weaponCoord->workm` in the contacts' view frame.
/// Enemy-body hits suppress surface selection. A selected eligible grid point
/// receives 0..7 units of XYZ jitter; only `impactCoord`'s three cached
/// translation words are written. Failure leaves that temporary node untouched.
/// On success, requests `SOUND_BULLET_IMPACT` synchronously and latches combat
/// noise. Scratch/GTE requirements follow the picker and sound APIs; the helper
/// neither releases nor retains the caller's writable impact node.
static inline void _weaponResolveShotImpact(GameActor* actor, const GfxCoord* weaponCoord, GfxCoord* impactCoord)
{
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if (playerActorSpawnWeaponImpact(actor->weaponContacts, weaponCoord, impactCoord) != 0) {
        worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
    }
}

/// Whether the player's weapon arm still plays its attack clip linearly.
///
/// Tests the arm's slot, the live character's second attachment parent part
/// (`gPlayerActorAttachmentParentParts`); a recovering attack ends once it stops.
static inline s32 _weaponIsArmSlotAdvancing(Task* playerTask)
{
    return playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0);
}

#endif /* SRC_WEAPONS_SHARED_ATTACK_H */
