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
/// The catalogue remains live with gameplay and owns no overlay pointers.
extern AttachmentLevelTable gAttachmentLevelTable;

extern u8 Gp_DebugAttachLevels[18];

/// Gameplay's shared attachment selection, casting and timed protection state.
///
/// The wheel, HUD, player and PE effects coordinate through this singleton.
/// Packed spell family/level and cast phases describe the current use; combo
/// bytes, immunity timers and wards persist independently across casts.
/// Rooms and effects borrow this gameplay storage rather than owning copies.
extern AttachmentState gAttachmentState;

/// Flag byte cleared by `attachmentCancel` / `playerActorSpawn`.
extern u8 D_80115768;

extern AttachmentAreaRow Gp_AttachParams[ATTACHMENT_AREA_ABILITY_COUNT][ATTACHMENT_AREA_LEVEL_COUNT];

#endif // GAMEPLAY_ATTACHMENTS_H
