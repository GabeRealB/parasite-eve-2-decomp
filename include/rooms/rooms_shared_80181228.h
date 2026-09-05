#ifndef ROOMS_SHARED_80181228_H
#define ROOMS_SHARED_80181228_H

#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

/// The 0x18-byte cutscene record a room hands the shared cutscene runner as
/// `Task::spawnArg2`: `field_0` is the save-data view the scene runs under
/// (negative means "restore only"), `field_1` the CAP slot, `field_2` skips
/// straight to the abort path, `field_3` the CAP file to load and `field_4` /
/// `field_8` / `field_C` the sound events for the start, the end and the
/// scene itself. `field_10` is the sub-task's spawn argument and `field_14` /
/// `field_16` the fade lengths.
typedef struct _RoomsShared80181228Rec {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} RoomsShared80181228Rec;

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
