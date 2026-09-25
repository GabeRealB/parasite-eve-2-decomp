#ifndef ROOMS_SHARED_80181228_H
#define ROOMS_SHARED_80181228_H

#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

/// The sub-task the runner spawns for the scene, and the descriptor it spawns
/// it from; `AreaRecs` is the area-record table the Dryfield hand-off applies.
/// Every carrying room holds its own three at its own addresses, named there
/// by the family's symbol maps, so the shared object owns no data.
extern Task*          RoomsShared80181228Task;
extern TaskDesc       RoomsShared80181228Desc;
extern GpAreaApplyRec RoomsShared80181228AreaRecs;

/// The rooms' cutscene runner: a 24-state machine that suppresses the player
/// and ally HUD, loads and starts the scene's CAP slot, lets confirm/cancel cut
/// the sub-task short, applies the story-flag side effects when the scene ends,
/// and restores everything before killing itself. Seven rooms carry this body.
void RoomsShared80181228(Task* task);

#endif // ROOMS_SHARED_80181228_H
