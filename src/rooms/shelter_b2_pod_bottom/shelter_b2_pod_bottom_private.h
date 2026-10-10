#ifndef SRC_ROOMS_SHELTER_B2_POD_BOTTOM_SHELTER_B2_POD_BOTTOM_PRIVATE_H
#define SRC_ROOMS_SHELTER_B2_POD_BOTTOM_SHELTER_B2_POD_BOTTOM_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/room.h"

/// Radius, lift and spread offsets for the arc flash's three bands, indexed 0..2.
///
/// Signed halfwords are distances in the effect's local coordinate units.
/// The drawer adds the live work's radius, lift and spread to these offsets;
/// positive lift places the raised rim along negative local Y. Borrowed while
/// this room overlay is loaded.
extern EffectBandShape gShelterB2PodBottomArcFlashBandShapes[3];

/// Eighteen energy-ring tint recipes: right-shift counts in red, green, blue order.
///
/// Row indices are 0..17 and each unsigned-halfword count is 0..2. Each channel
/// is the current signed brightness shifted by its count, then narrowed to a
/// byte. Random selection chooses one whole row; this table remains borrowed
/// while the room overlay is loaded.
extern u16 gShelterB2PodBottomEnergyRingChannelShifts[18][3];

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
