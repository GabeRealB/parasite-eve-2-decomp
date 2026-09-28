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

s32 Stage_SetEndingFlag(void);

void Stage_ReleasePrimBuf(void);

/// Overlay callers pass 1; the argument is unused.
void Stage_RequestSpecialFlag(s32 unused);

s32 Stage_BeginTransition(s32 arg0, s32 arg1);

s32 Stage_BeginTransitionKind7(s32 arg0);

s32 Stage_SetFadeRate(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

void Stage_SetFadeMax(u8 arg0);

void Stage_InitPrimBufOnce(void);

s32 Stage_HasTransitionFlags(void);

/// Sets StageCtx::field_1c 0x20000000; Display_TransitionTask services it
/// with Gfx_StoreImageSlot and clears the bit.
s32 Stage_RequestImageCapture(void);

void Stage_RequestFromAreaTable(s32 arg0);

void Stage_RequestMidiFromMap(s32 arg0);

s32 Stage_GetFadeStatus(void);

void Stage_InitOtOnce(void);

s32 Stage_GetModeByte12(void);

void Stage_ResetFade(void);

#endif // MAIN_STAGE_H
