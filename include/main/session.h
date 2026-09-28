#ifndef MAIN_SESSION_H
#define MAIN_SESSION_H

#include "types.h"

#include "main/session_types.h"

struct Task;

/// The first four bytes of a place key - view, room, area and stage - read as
/// one word, which is how code tests for a place. The key is byte-aligned, so
/// this is a view of its bytes, not a member; it is only ever applied to keys
/// that sit on a word boundary. Build the value to compare with `GP_LOC_KEY`
/// and select the bytes that matter with `GP_LOC_STAGE_AREA` or another mask.
#define GP_LOC_WORD(key) (*(u32*)&(key))

/// `GP_LOC_WORD` of a place: the stage in the high byte, then the area, the
/// room and the view.
#define GP_LOC_KEY(stage, area, room, view) \
    (((u32)(stage) << 24) | ((u32)(area) << 16) | ((u32)(room) << 8) | (u32)(view))

/// Masks of `GP_LOC_WORD` selecting the bytes a test compares.
#define GP_LOC_STAGE_AREA GP_LOC_KEY(0xFF, 0xFF, 0, 0)

#define GP_LOC_AREA_VIEW GP_LOC_KEY(0, 0xFF, 0, 0xFF)

/// Pointer to the live `GameSession`.
extern GameSession* gGameSession;

extern s32 D_8005ED68;

extern s32 D_8005ED8C;

/// Stores a task in the session's pointer-slot table.
void Game_SetPtrSlot(void* ptr, s32 index);

/// Returns the task the session keeps in a pointer slot.
///
/// A slot is how the session holds on to a task past the call that spawned it:
/// the task is filed under a slot number, and the code that files it and the
/// code that reads it agree on what that number means. The table itself records
/// nothing about a slot's meaning.
///
/// @param slot Slot number, 0 to 15.
/// @return The task in that slot, or `NULL` while the slot is empty.
struct Task* gameGetPtrSlot(s32 slot);

/// Empties every pointer slot.
void Game_ClearPtrSlots(void);

#endif // MAIN_SESSION_H
