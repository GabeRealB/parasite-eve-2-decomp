#ifndef GAMEPLAY_WORLD_TARGETS_H
#define GAMEPLAY_WORLD_TARGETS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "types.h"

#include "gameplay/item_pickup.h"
#include "gameplay/world_state.h"

#include "main/session_types.h"
#include "main/task_types.h"

extern GpStateF0 Gp_StateF0;

void func_800DA6E8(void* arg0, s32 arg1, s32 arg2);

/// Detaches `node` from every actor slot locked onto it and takes it off the
/// tracked list.
void Gp_UnlinkNode(GpLinkNode* node);

/// Appends `node` to the tracked list when it is not already on it, and marks
/// it lockable.
void Gp_LinkNode(GpLinkNode* node);

/// Two-bit mask of `Gp_ActorSlots[]`: the slots whose actor is locked onto
/// `node`.
s32 Gp_NodeSlotMask(GpLinkNode* node);

/// Locks actor slot 0 onto `node`, releasing whichever node held it, and marks
/// `node` lockable.
void Gp_AssignNodeSlot0(GpLinkNode* node);

/// Detaches `node` from every actor slot and marks it un-lockable, leaving it
/// on the tracked list.
void Gp_ClearNodeSlots(GpLinkNode* node);

void* Gp_FindLockNode(Task* arg0);

void* Gp_FindLockNodePad(Task* arg0);

void Gp_GetLockPos(GpLinkNode* arg0, VECTOR3* out);

s32 Gp_LoadActorImage(Task* arg0, GpImgRec* arg1, RECT* arg2);

void Gp_LoadImages(GpImgRec* arg0);

void Gp_ArmStateF0(s32 arg0);

void Gp_SetStateF0Bit(s32 arg0);

void Gp_SetStateF0Byte3(s32 arg0);

void Gp_IncStateF0Ref(s32 arg0);

void Gp_ReleaseStateF0Add(Task* arg0, s32 arg1);

void Gp_ReleaseStateF0Clear(void);

void Gp_ReleaseStateF0(Task* arg0, s32 arg1);

#endif // GAMEPLAY_WORLD_TARGETS_H
