#ifndef MAIN_PRIVATE_STAGE_H
#define MAIN_PRIVATE_STAGE_H

#include "types.h"

/// Selects the scene-event column within each area's stage music row.
///
/// The low byte of `stage` must be 0..5; 0 selects the default column and
/// callers normally supply `GAME_STAGE_ACROPOLIS` through
/// `GAME_STAGE_SHELTER_NEO_ARK`. The scene event is compared as a signed byte,
/// while the resulting column is checked and returned as an unsigned byte.
///
/// Stages 1 and 2 use the event directly. Stage 3 folds events from 9 onward
/// into columns 9 or 10. Stages 4 and 5 translate later events relative to
/// `sceneEventBase - 1` (the caller supplies 9 and 20 respectively), with
/// event-specific overrides. A column outside that stage's row returns 0.
s32 stageMusicSelectColumn(s32 stage, s32 sceneEvent, s32 sceneEventBase);

/// Starts the resident controller for the queued stage mode.
///
/// Requires a context queued by `displayQueueModeTask`, initialized display
/// environments with OT index 0 or 1, and finished GPU use of the task buffers.
/// The old display list must be empty or its tasks already released. Binds small
/// task buffers, takes transition ownership, holds flips and selects the
/// framebuffer opposite the OT index before spawning the bodyless controller.
/// Leaves the new display list selected for subsequent spawns. Allocation
/// failure is ignored and still leaves ownership and the empty list changed.
void stageStartModeController(void);

#endif // MAIN_PRIVATE_STAGE_H
