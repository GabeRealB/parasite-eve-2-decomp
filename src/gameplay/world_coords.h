#ifndef GAMEPLAY_PRIVATE_WORLD_COORDS_H
#define GAMEPLAY_PRIVATE_WORLD_COORDS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/room.h"

/// Persistent player directional-light coefficients, with 12 fractional bits.
///
/// The player lighting pass fills this MATRIX; the player and its attachment
/// and equipment models borrow the same storage while gameplay is loaded.
extern MATRIX gWorldCoordPlayerLightMatrix;

/// Persistent player RGB light-colour matrix and ambient translation.
///
/// The 3x3 coefficients have 12 fractional bits; ambient RGB uses 16 units
/// per 8-bit colour level. Lighting fills it and attachment/status pulses
/// replace directional coefficients. Player and child models borrow it.
extern MATRIX gWorldCoordPlayerColorMatrix;

/// Persistent companion directional-light coefficients, with 12 fractional bits.
///
/// Lighting fills this separate pair before companion child models borrow it.
/// The MATRIX remains live while gameplay is loaded.
extern MATRIX gWorldCoordCompanionLightMatrix;

/// Persistent companion RGB light-colour matrix and ambient translation.
///
/// The 3x3 coefficients have 12 fractional bits; ambient RGB uses 16 units
/// per 8-bit colour level. Companion and child models borrow this storage
/// while gameplay is loaded, independently of the player pair.
extern MATRIX gWorldCoordCompanionColorMatrix;

/// NUL-terminated question mark marking the projected diagnostic light sample.
///
/// The last two stored bytes follow the terminator and are not drawn; their
/// purpose is unproven. The text is owned by the gameplay image.
extern const u8 gWorldCoordLightProbeMarkerText[4];

/// Fallback room-view RGB ambient minima, one 8-bit colour level above black.
///
/// RGB and stored luma are all 16, in units of 16 per 8-bit colour level.
/// Used when the room lighting record or ambient table is absent, or the view
/// exceeds its count; also copied as a complete eight-byte default entry.
/// This value lives with gameplay and does not borrow a room overlay.
extern WorldCoordRoomAmbientEntry gWorldCoordDefaultRoomAmbientEntry;

/// Selects the stored ambient RGB replacement when exactly 1; 0 disables it.
///
/// The setter and player-light initialization disable the gate without clearing
/// `gWorldCoordAmbientColorOverride`. Storage is a signed byte, matching the
/// lighting query's exact signed-byte comparison.
extern s8 gWorldCoordAmbientColorOverrideEnabled;

/// Copied signed RGB replacement for a model's ambient colour-matrix translation.
///
/// vx/vy/vz use 16 units per 8-bit colour level. The complete SVECTOR is copied,
/// including its unused pad halfword; no caller pointer is retained. The value
/// persists while disabled and is consumed only when the override gate is 1.
extern SVECTOR gWorldCoordAmbientColorOverride;

/// Stage-to-area-to-room model-lighting lookup tables borrowed from loaded overlays.
///
/// Index with `GameLocationKey.stage - 1` (stages 1..5: Akropolis, Dryfield day,
/// Dryfield night, Mine/Shelter, Shelter/Neo Ark), then `area - 1`, then `room - 1`.
/// The stage area counts are 20, 38, 38, 49 and 33; each area fixes its room
/// extent. Stage and area pointers may be NULL, yielding no descriptor. Other
/// indices must be valid: the lookup performs no range checks.
/// Gameplay owns these five pointers. The selected map overlay must be loaded
/// for its tables/local descriptors; room-light records and arrays require the
/// corresponding room overlay and must not survive its unload.
extern WorldCoordRoomLighting** gWorldCoordRoomLightingTables[5];

/// Copies RGB multipliers for the model light-colour matrix, or disables the override.
///
/// `colorScales` supplies an entire readable SVECTOR: vx/vy/vz hold raw
/// unsigned Q12 bits for red/green/blue (zero suppresses, ONE is unity). The
/// full eight-byte value is copied, including its unused final halfword; no
/// caller pointer is retained. NULL disables use of the stored value. Scaling
/// affects the three colour rows and preserves the ambient translation.
void worldCoordSetLightColorScaleOverride(const SVECTOR* colorScales);

#endif // GAMEPLAY_PRIVATE_WORLD_COORDS_H
