#ifndef MAIN_STAGE_H
#define MAIN_STAGE_H

#include "types.h"

#include "main/stage_types.h"

extern StageMusicParams gStageMusicParams;

/// 0 while a music-load task runs, 0xFF once it has finished or given up; code
/// that must wait for the music checks it.
extern u8 gStageMusicLoadState;

/// The music-table entry a scene selects. A load task spawned with argument 2
/// plays it instead of the area's own entry; rooms and actors set it.
extern u8 gStageSceneMusicEntry;

/// A song a room started itself, outside the music table. Music-volume
/// changes are applied to it too, and the next load stops it and clears this.
extern u8 gStageRoomSong;

/// Requests the active mode task's reload-and-exit path, returning 0.
///
/// The transition task handles this after outstanding view/file transitions,
/// reloads room resources when its entry mode requires them, resumes a suspended
/// movie and returns presentation to the game loop. This call neither exits
/// the caller's task nor waits for the reload.
s32 stageRequestModeTaskExit(void);

void Stage_ReleasePrimBuf(void);

/// Overlay callers pass 1; the argument is unused.
void Stage_RequestSpecialFlag(s32 unused);

s32 Stage_BeginTransition(s32 arg0, s32 arg1);

s32 Stage_BeginTransitionKind7(s32 arg0);

s32 Stage_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

void Stage_SetFadeMax(u8 arg0);

void Stage_InitPrimBufOnce(void);

s32 Stage_HasTransitionFlags(void);

/// Queues capture of the framebuffer presented when the mode task handles the request.
///
/// Requires an active mode task. Its transition handler captures the then-current
/// stage/area frame into that area's RAM image slot after higher-priority work,
/// then clears the request. Repeated requests coalesce. Returns 0 immediately;
/// neither this return nor the call itself means GPU readback has completed.
s32 stageRequestFrameCapture(void);

void Stage_RequestFromAreaTable(s32 arg0);

void Stage_RequestMidiFromMap(s32 arg0);

/// Classification of the stage fade overlay's current level.
enum {
    STAGE_FADE_CLEAR   = 0,
    STAGE_FADE_AT_MAX  = 1,
    STAGE_FADE_BETWEEN = -1,
};

/// Returns `STAGE_FADE_CLEAR`, `STAGE_FADE_AT_MAX` or `STAGE_FADE_BETWEEN`.
///
/// Zero level takes precedence even when the maximum is zero. This classifies
/// the level without advancing the fade or testing whether its step is stopped.
s32 stageGetFadeStatus(void);

void Stage_InitOtOnce(void);

/// Reads the latch recording whether a file-load transition cleared both framebuffers.
///
/// Returns 1 after the clears and GPU synchronization. Mode-task initialization
/// and a subsequent view-transition capture clear the latch to 0; this does not
/// inspect the current framebuffer contents.
s32 stageGetLoadBuffersCleared(void);

void Stage_ResetFade(void);

#endif // MAIN_STAGE_H
