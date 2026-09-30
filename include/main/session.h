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

/// Packs a location prefix or area-layout selector into a `u32` comparison key.
///
/// Bytes from high to low are stage, area, room or placement variant, and
/// room-local view slot. Grant keys use the placement variant and a zero view;
/// location prefixes exclude warp and placement variant. Zero bytes are literal
/// zeros; callers must mask the bytes they want to ignore.
///
/// Arguments must be integer byte values in 0..255. Each is evaluated once,
/// in unspecified order, and converted to `u32` before shifting, without byte
/// masking or truncation. Constant arguments produce an integer constant
/// expression suitable for case labels, static initializers and byte masks.
#define GAME_LOCATION_KEY(stage, area, roomOrVariant, view) \
    (((u32)(stage) << 24) | ((u32)(area) << 16) | ((u32)(roomOrVariant) << 8) | (u32)(view))

/// Selects the stage and area bytes of a packed location prefix.
#define GAME_LOCATION_STAGE_AREA_MASK GAME_LOCATION_KEY(0xFF, 0xFF, 0, 0)

/// Selects the area and room-local view bytes of a packed location prefix.
#define GAME_LOCATION_AREA_VIEW_MASK GAME_LOCATION_KEY(0, 0xFF, 0, 0xFF)

/// Resident live world, input and script state shared by main and the overlays.
///
/// Always points to the same `GameSession`, whose storage survives heap resets
/// and overlay replacement. New-game, load and reset paths clear the object in
/// place. Consumers borrow this object and must not release its storage.
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
