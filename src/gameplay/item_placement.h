#ifndef GAMEPLAY_PRIVATE_ITEM_PLACEMENT_H
#define GAMEPLAY_PRIVATE_ITEM_PLACEMENT_H

#include "main/session_types.h"

/// Spawns every matched object in the destination area's flag-bank place list.
///
/// Borrows a location with stage 0..5 and an area within that stage's loaded
/// room table. Missing room/place tables do nothing. Live place and spawn
/// tables require their respective END records; a nonempty place list requires
/// a spawn table. Uses the first matching kind even when allocation fails,
/// without consulting the object's saved two-bit state. New tasks own their
/// enemy work and become children of the scene manager; callbacks and model
/// resources must stay loaded. Bodyless objects receive no placement update.
/// Model-backed objects receive the place key, kind and root XYZ in signed game
/// units. Yaw narrows to signed halfwords in 4096 units per turn; zero records zero without
/// replacing the matrix rotation. Placement invalidates root composition.
void areaSpawnRoomObjects(const GameLocationKey* location);

#endif // GAMEPLAY_PRIVATE_ITEM_PLACEMENT_H
