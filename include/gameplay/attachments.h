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

extern AttachmentState Gp_StateC08;

/// Flag byte cleared by `attachmentCancel` / `Gp_SpawnPlayer`.
extern u8 D_80115768;

extern AttachmentAreaRow Gp_AttachParams[ATTACHMENT_AREA_ABILITY_COUNT][ATTACHMENT_AREA_LEVEL_COUNT];

#endif // GAMEPLAY_ATTACHMENTS_H
