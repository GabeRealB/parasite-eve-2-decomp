/* What the weapons' attack states share: the impact of a shot from the guns whose rounds end in the common bullet
 * impact (the M93R, M950, P08, M249 and the M4A1 with its Bayonet, Hammer and Pyke), and the test that ends an
 * attack's recovery.
 */

#ifndef WEAPONS_ATTACK_H
#define WEAPONS_ATTACK_H

#include <gameplay/player_actor.h>
#include <gameplay/world_collision_types.h>
#include <gameplay/world_coords.h>

#include <engine/mc.h>
#include <engine/mc_types.h>
#include <engine/session_types.h>
#include <engine/sound_ids.h>

/// Ends a shot's contact tests and spawns the impact of the surface it hit at `impactCoord`, jittered by up to 7 units,
/// playing `SOUND_BULLET_IMPACT` there; a hit on an enemy spawns none.
static inline void _weaponResolveShotImpact(GameActor* actor, const GfxCoord* weaponCoord, GfxCoord* impactCoord)
{
    actor->collisionBodies[GAME_ACTOR_BODY_WEAPON].flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
    if (playerActorSpawnWeaponImpact(actor->weaponContacts, weaponCoord, impactCoord) != 0) {
        worldCoordPlaySound(impactCoord, SOUND_BULLET_IMPACT, 1);
    }
}

/// Whether the player's weapon arm still plays its attack clip; a recovering attack ends once it stops.
static inline s32 _weaponIsArmSlotAdvancing(Task* playerTask)
{
    return playerActorIsSlotAdvancingLinearly(playerTask, gPlayerActorAttachmentParentParts[gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.characterId][1], 0, 0);
}

#endif // WEAPONS_ATTACK_H
