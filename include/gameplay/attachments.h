#ifndef GAMEPLAY_ATTACHMENTS_H
#define GAMEPLAY_ATTACHMENTS_H

#include "types.h"

#include "gameplay/attachment_state.h"

// Attachment parameters, combination state and menu support.

extern GpIdParamTable Gp_IdParamHi;

extern u8 Gp_DebugAttachLevels[18];

extern AttachmentState Gp_StateC08;

/// Flag byte cleared by `func_800A7DE0` / `Gp_SpawnPlayer`.
extern u8 D_80115768;

extern GpAttachParam Gp_AttachParams[55];

#endif // GAMEPLAY_ATTACHMENTS_H
