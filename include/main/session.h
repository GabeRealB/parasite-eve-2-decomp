#ifndef MAIN_SESSION_H
#define MAIN_SESSION_H

#include "types.h"

#include "main/session_types.h"

struct Task;

/// Reads a location's stage, area, room and view as a `u32` comparison value.
///
/// The PS1 little-endian representation puts stage in bits 31..24, area in
/// 23..16, room in 15..8 and view in 7..0, matching `GAME_LOCATION_KEY`.
/// Warp and variant are excluded. Mask the bytes a comparison should ignore.
///
/// `key` must be an addressable `GameLocationKey` whose first four bytes are
/// initialized and whose address is word-aligned; the key type itself is only
/// byte-aligned. Evaluates `key` once and exposes a read-only `const u32`
/// lvalue. The pointer cast deliberately views the four-byte representation
/// prefix; the six-byte key retains its layout and byte alignment.
#define GAME_LOCATION_WORD(key) (*(const u32*)&(key))

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

/// Masks a packed location value to identify its stage and area.
///
/// The `u32` integer constant expression `0xFFFF0000` retains stage in bits
/// 31..24 and area in bits 23..16, clearing the room and view bytes. Compare
/// `GAME_LOCATION_WORD(key) & GAME_LOCATION_STAGE_AREA_MASK` with
/// `GAME_LOCATION_KEY(stage, area, 0, 0)` to match any room or view in that area.
/// Warp and placement variant are outside the packed word and do not affect it.
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

/// Registers a borrowed task in one resident session slot, or clears that slot.
///
/// `slot` is an element index in 0..15 and is not checked. NULL `task` empties
/// the slot. The store replaces any previous registration. It does not retain
/// the task, and task exit does not clear the slot, so a non-NULL registration
/// may be used only while that task is alive. `gameClearTaskSlots` empties
/// every slot when the task system resets.
///
/// The player and companion slots are the spawn-time handles of those tasks,
/// separate from `gPlayerActorTasks`, which those actors publish on their
/// first tick.
void gameSetTaskSlot(struct Task* task, s32 slot);

/// Established task registrations in the resident session's 16-slot table.
///
/// Player and companion registrations are available at spawn time, before
/// their first task tick. The room and room-effect registrations select the
/// current room's controllers. Unlisted indices have no established role.
enum {
    GAME_TASK_SLOT_VIEW_GATE = 1,
    /// Session task-table index for the spawned player actor.
    ///
    /// A successful spawn registers the task before its first tick, with a
    /// TMD body and `GameActor` work. This borrowed registration is replaced
    /// by the next successful spawn and cleared by `gameClearTaskSlots`;
    /// task exit does not clear it. Use it only while that task remains alive.
    /// `gPlayerActorTasks` publishes the player separately during its first tick.
    GAME_TASK_SLOT_PLAYER = 3,
    /// Session task-table index for the current scene manager.
    ///
    /// Registered when spawned, before area actors are created. Top-level
    /// enemy tasks join its child ring; it routes child lookups and actor
    /// broadcasts once its first tick installs the message table. Registration
    /// does not retain the task and is cleared by `gameClearTaskSlots`, not by
    /// task exit. Use it only while the manager is live.
    ///
    /// Event-script sends use the following operand as a placed-actor index
    /// (0..15); -1 addresses the manager itself.
    GAME_TASK_SLOT_SCENE       = 4,
    GAME_TASK_SLOT_ROOM_EFFECT = 5,
    GAME_TASK_SLOT_CAP_CONTROL = 6,
    GAME_TASK_SLOT_ROOM        = 7,
    GAME_TASK_SLOT_COMPANION   = 10
};

/// Borrows the task registered in a resident session slot, or NULL if empty.
///
/// `slot` is an element index in 0..15; it is not checked. Registration does
/// not retain a task or clear itself when the task exits, so a non-NULL result
/// may be used only while the registered task is alive. `gameSetTaskSlot`
/// replaces a registration and `gameClearTaskSlots` clears them on task-system
/// reset. The getter neither transfers ownership nor changes the table.
///
/// The player and companion slots are separate from `gPlayerActorTasks`,
/// which those actors publish during their first tick.
///
struct Task* gameGetTaskSlot(s32 slot);

/// Clears all 16 borrowed task registrations in the resident session.
///
/// Requires a live `gGameSession`. No exit handlers run and no tasks or resources
/// are released. Session resets clear these handles before discarding the task
/// list and reinitializing its heap; task exit alone leaves registrations intact.
void gameClearTaskSlots(void);

#endif // MAIN_SESSION_H
