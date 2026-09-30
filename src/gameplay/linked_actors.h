#ifndef GAMEPLAY_PRIVATE_LINKED_ACTORS_H
#define GAMEPLAY_PRIVATE_LINKED_ACTORS_H

#include "types.h"

#include "attachment_state.h"

#include "gameplay/world_targets_types.h"

#include "main/session_types.h"

/// Head of the list of enemies the lock-on system tracks; `Gp_ResetLinkState`
/// empties it.
extern WorldTargetNode* Gp_LinkList;

void func_800A4904(s32 arg0);

void Gp_DrawAimCircle(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

void Gp_InitSlot18(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

void func_800A5574(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

void func_800A57B0(GpIdMapC* arg0);

void func_800A63B4(s32 arg0, s32 arg1, s32 arg2);

#endif // GAMEPLAY_PRIVATE_LINKED_ACTORS_H
