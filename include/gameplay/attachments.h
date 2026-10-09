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

extern AttachmentLevelTable Gp_IdParamHi;

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
