#ifndef MAIN_SESSION_H
#define MAIN_SESSION_H

#include "types.h"

#include "main/session_types.h"

struct Task;

/// Reads the view/room/area/stage prefix as one PS1 little-endian word.
///
/// Requires an addressable, word-aligned `GameLocationKey` instance. Evaluates
/// `key` once; warp and variant are outside this four-byte view.
#define GAME_LOCATION_WORD(key) (*(u32*)&(key))

/// Packs four byte values into a location prefix, stage in the high byte.
///
/// Each argument is evaluated once and must be in 0..255. Conversion to u32
/// precedes each shift; values are not masked or truncated by this helper.
#define GAME_LOCATION_KEY(stage, area, room, view) \
    (((u32)(stage) << 24) | ((u32)(area) << 16) | ((u32)(room) << 8) | (u32)(view))

/// Selects the stage and area bytes of a packed location prefix.
#define GAME_LOCATION_STAGE_AREA_MASK GAME_LOCATION_KEY(0xFF, 0xFF, 0, 0)

/// Selects the area and room-local view bytes of a packed location prefix.
#define GAME_LOCATION_AREA_VIEW_MASK GAME_LOCATION_KEY(0, 0xFF, 0, 0xFF)

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
