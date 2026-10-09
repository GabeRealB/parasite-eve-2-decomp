#ifndef SRC_ROOMS_SHELTER_B2_POD_BOTTOM_SHELTER_B2_POD_BOTTOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B2_POD_BOTTOM_SHELTER_B2_POD_BOTTOM_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/room.h"

extern EffectBandShape D_shelter_b2_pod_bottom_80181C94[3];

extern u16 D_shelter_b2_pod_bottom_80181CA8[18][3];

/// Facing-gated exit from departure slot 1 to the B2 pod access tunnel's arrival slot 2.
///
/// One mutable action trigger, including the LAST marker. Geometry uses room
/// coordinates and Q12 normals; setup binds and links it to the active view.
/// Keep the room overlay loaded until this trigger is unlinked.
extern WorldCollisionTrigger gShelterB2PodBottomActionTriggers[1];

/// Minimum ambient colours for logical views 1..16.
///
/// Entry zero stores the count; each subsequent entry belongs to that 1-based
/// view. RGB uses 16 units per 8-bit colour level. Lighting reads the table
/// while this overlay is loaded and uses default minima above its count.
extern WorldCoordRoomAmbientEntry gShelterB2PodBottomAmbientByView[17];

#endif // SRC_ROOMS_SHELTER_B2_POD_BOTTOM_SHELTER_B2_POD_BOTTOM_PRIVATE_H
