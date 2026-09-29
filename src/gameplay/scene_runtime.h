#ifndef GAMEPLAY_PRIVATE_SCENE_RUNTIME_H
#define GAMEPLAY_PRIVATE_SCENE_RUNTIME_H

#include "types.h"

#include "gameplay/animation.h"
#include "area_flags.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"

extern GpBit2Bank Gp_Bit2Banks[];

extern TaskDesc D_8010D1FC;

void func_800B25B0(void);

void Gp_EnqueueSndCd(u8 arg0);

void Gp_AnimTickSlot2(GpAnimCtx* arg0, GpAnimSlot* arg1);

void Gp_AnimPlaySlot(GpAnimCtx* arg0, s32 arg1, GpAnimPose* arg2, u16 arg3, s32 arg4, s32 arg5, s32 arg6,
                     void* arg7);

void Gp_ApplyAreaTmdFlags(void);

void Gp_ReparentCoord(GpCoord* arg0, GpCoord* arg1);

void Gp_SetAreaFlag2(s32 arg0, GpAreaKey* arg1);

void Gp_SetAreaFlag0(GpAreaKey* arg0);

void Gp_FreeSlot4TmdBuffers(void);

#endif // GAMEPLAY_PRIVATE_SCENE_RUNTIME_H
