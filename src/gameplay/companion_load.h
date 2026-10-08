#ifndef GAMEPLAY_PRIVATE_COMPANION_LOAD_H
#define GAMEPLAY_PRIVATE_COMPANION_LOAD_H

#include "types.h"

#include "actor.h"
#include "gameplay/actor_spawn_types.h"

#include "main/session_types.h"
#include "main/task_types.h"

/// Selects the companion scheduled for the live save's destination area.
///
/// Prioritizes families 2, 1, then 3 and writes the save's family/variant.
/// Returns 0 when no resource load is needed, or 1..3 for the family to load;
/// the caller updates the session's family cache after a nonzero result.
/// Family 1 also updates its session variant cache when a reload is needed.
/// No match clears both caches and returns 0, without releasing resources.
/// Requires a live save/session, schedule indices 0..10 for families 1/2 and
/// 0..1 for family 3, and a valid one-based area in the selected stage table.
s32 companionSelectForArea(void);

/// Sets first-character sound-bank retention from the destination companion schedules.
///
/// Uses the live save's stage/area and the schedule bounds of
/// `companionSelectForArea`; the live session's nighttime Water Hole forces
/// retention off. Call before resetting sound for the area. Changes retention
/// policy only; it neither loads nor releases a bank immediately.
void companionConfigureSoundBankRetention(void);

void Gp_SetupCompanionActor(const ActorSpawnTransform* spawnTransform, ActorSpawnOptions* options);

/// Marks the saved stage and area visited, requesting a pose reset on the area's first visit.
///
/// Borrows a word-aligned location with stage 1..5 and area 1..64 valid for
/// that stage's loaded area tables. Updates the live save and stage bank;
/// repeat visits preserve area pose flags. The debug hook runs only when the
/// stage's visit bit is first set. Does not modify the location or clear a bank.
void areaMarkVisited(const GameLocationKey* location);

/// Holds drawing for session-reload phase 1, then advances to the rebuild phase.
///
/// Requires the live reload task. Sets the active display hold and skips
/// drawing; `GAME_FLOW_RELOAD_BLANK_DISPLAY` also clears the image source.
/// Other display-mode nibbles preserve the current source.
void gameFlowHoldSessionDisplayTask(Task* task);

/// Prepares session-reload phase 0, aborting the task while death presentation is active.
///
/// Disables character sound-script requests before testing death. Otherwise
/// requests battle-escape handling unless `GAME_FLOW_RELOAD_SKIP_BATTLE_ESCAPE`
/// is set, including a finished-to-resumed battle transition, then advances
/// to the display-hold phase. Task teardown can invalidate `task` immediately.
void gameFlowPrepareSessionReloadTask(Task* task);

void Gp_LoadFinishTask(Task* task);

void Gp_LinkRoomObjectsSpawn(Task* task);

#endif // GAMEPLAY_PRIVATE_COMPANION_LOAD_H
