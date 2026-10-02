#ifndef SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H

#include "types.h"

#include "gameplay/gpu_image_upload.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern GpuImageUpload D_dryfield_breezeway_80183144[2];

extern SVECTOR D_dryfield_breezeway_80183164;

extern Task* D_dryfield_breezeway_801843C0;

extern TaskDesc gRoomEventTaskDesc;

extern TaskMessageEntry D_dryfield_breezeway_80181DE0[6];

extern TaskDesc D_dryfield_breezeway_80181E10[2];

extern TaskDesc D_dryfield_breezeway_801820B0[2];

extern TaskDesc D_dryfield_breezeway_80182E18;

void func_dryfield_breezeway_8017DC3C(Task* arg0);

// Callbacks referenced by the overlay's shared data tables.

s32 func_dryfield_breezeway_8017D90C(Task*, s32, s32, s32);

s32 func_dryfield_breezeway_8017D940(Task*, s32, RoomEventMsg*, RoomEventMsg*);

s32 func_dryfield_breezeway_8017DA48(Task*, s32, s32, s32);

s32 func_dryfield_breezeway_8017DBA4(Task*, s32, s32, s32);

s32 func_dryfield_breezeway_8017DBD8(Task*, s32, RoomEventMsg*, RoomEventMsg*);

void func_dryfield_breezeway_8017DCE4(Task*);

#endif // SRC_ROOMS_DRYFIELD_BREEZEWAY_DRYFIELD_BREEZEWAY_PRIVATE_H
