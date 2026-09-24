#ifndef ROOMS_NEO_ARK_BRIDGE_H
#define ROOMS_NEO_ARK_BRIDGE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// State handlers of the room's entry task, indexed by its state through
/// `func_neo_ark_bridge_8017E8FC`: set-up, idle, then kill.
extern const TaskFuncTable3 D_neo_ark_bridge_8017D614;

void func_neo_ark_bridge_8017F0C4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2);
void func_neo_ark_bridge_8017F8B4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_neo_ark_bridge_8017FCA0(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);
void func_neo_ark_bridge_80180228(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_neo_ark_bridge_80180654(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_neo_ark_bridge_80180ED8(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_neo_ark_bridge_80181558(GsCOORDINATE2* arg0, s16 arg1, u8* rgb);

#endif // ROOMS_NEO_ARK_BRIDGE_H
