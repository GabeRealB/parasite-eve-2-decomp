#ifndef GAMEPLAY_ATTACHMENTS_H
#define GAMEPLAY_ATTACHMENTS_H

#include "types.h"

#include "gameplay/attachment_state.h"

// Attachment parameters, combination state and menu support.

/// Returns 1 in the shooting gallery's alternate Parasite Energy training mode.
///
/// Requires Acropolis's M.I.S.T. shooting-gallery area and player resource
/// variant 4; room and view do not affect the test. This mode uses the
/// separate training ability levels and disables the ordinary PE release menu.
/// Returns 0 everywhere else and does not change session or player state.
s32 attachmentIsTrainingMode(void);

/// Read-only parameters for eighteen attachment abilities at levels 1..3.
///
/// Row zero is empty; ability slot 0..17 at level 1..3 selects slot * 3 + level
/// in the 55-row table. Attachment attack keys select the same valid row with
/// their low seven bits. Rows supply EXP/MP costs, cast timing and hit/healing
/// parameters; the byte view addresses the same unsigned halfword columns.
/// The catalog remains live with gameplay and owns no overlay pointers.
extern AttachmentLevelTable gAttachmentLevelTable;

/// Separate learned spell levels used by M.I.S.T. shooting-gallery training.
///
/// Slots 0..11 are fire/wind/water/earth groups of three spells, storing levels
/// 0..3. Training resets those twelve and grants selected level-one spells.
/// The eighteen-byte backing object remains live with gameplay; its final six
/// bytes are not read or initialized by the known training consumers and have
/// no established role. Ordinary saved levels are unaffected.
extern u8 gAttachmentTrainingLevels[ATTACHMENT_ABILITY_COUNT];

/// Gameplay's shared attachment selection, casting and timed protection state.
///
/// The wheel, HUD, player and PE effects coordinate through this singleton.
/// Packed spell family/level and cast phases describe the current use; combo
/// bytes, immunity timers and wards persist independently across casts.
/// Rooms and effects borrow this gameplay storage rather than owning copies.
extern AttachmentState gAttachmentState;

/// Target-area and self-effect parameters indexed by ability slot and level minus one.
///
/// Slots 0..17 cover the twelve wheel spells and six item abilities; levels
/// are 1..3. Unlike `gAttachmentLevelTable`, this table has no empty leading
/// row. Region dimensions use hundreds of world units. Metabolism, Antibody
/// and Energy Shot read the combo view's unsigned frame duration. Gameplay
/// owns the table; PE overlays borrow it while their effects run.
extern AttachmentAreaRow gAttachmentAreaTable[ATTACHMENT_ABILITY_COUNT][ATTACHMENT_LEVEL_COUNT];

#endif // GAMEPLAY_ATTACHMENTS_H
