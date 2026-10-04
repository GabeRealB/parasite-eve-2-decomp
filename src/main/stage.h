#ifndef MAIN_PRIVATE_STAGE_H
#define MAIN_PRIVATE_STAGE_H

#include "main/task_types.h"

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

void Stage_InitOtAndSpawn(void);

void Stage_TaskExit(Task* task);

#endif // MAIN_PRIVATE_STAGE_H
