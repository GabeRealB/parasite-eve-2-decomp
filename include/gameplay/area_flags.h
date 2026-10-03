#ifndef GAMEPLAY_AREA_FLAGS_H
#define GAMEPLAY_AREA_FLAGS_H

#include "common.h"

#include "main/task_types.h"

/// `flagIndex` value that ends an `AreaObjectPlace` list.
enum { AREA_OBJECT_PLACE_END = 0xFFFF };

/// Low two bits of `AreaObjectPlace.state`, the slot's initial flag value.
enum { AREA_OBJECT_PLACE_STATE_MASK = 3 };

/// Bit 9 of `AreaObjectPlace.state`.
///
/// Published with the rest of `state`. The pickup prompt receives spawn
/// argument 1 when the bit is set and 0 when it is clear; the ordinary
/// pickup prompt collects the object immediately for argument 1.
enum { AREA_OBJECT_PLACE_PROMPT = 0x200 };

/// One object in a room's flag-bank list, ended by `AREA_OBJECT_PLACE_END`.
///
/// The record seeds that slot's packed 2-bit flag and, when `kind` matches
/// an enemy descriptor, supplies the spawn's place key, work type and
/// transform. `flagIndex` selects the flag — sixteen 2-bit values per word,
/// the low nibble the pair and the upper bits the word — and is stored in
/// the low half of the enemy place key. `placeKeyHigh` is shifted into that
/// key at bit 8, where the key keeps its stage and placement index; the
/// stored tables leave it zero. `kind` is a bank in the high byte and a
/// subtype in the low. Bank 0 is published as an item id, banks 0 and 1 open
/// the ordinary pickup prompt, and bank 8 is stored as the save point. The
/// spawn matches the whole word against the room's enemy-descriptor ids and
/// copies it to the enemy's work type. `state` contributes its low two bits
/// as the flag's initial value. Bit 8 of `state` is stored and published;
/// nothing reads it.
typedef struct {
    u16 flagIndex;    // Flag-bank index and place-key low half. AREA_OBJECT_PLACE_END ends the list
    u16 kind;         // Bank in the high byte, subtype in the low
    u16 placeKeyHigh; // Place key at bit 8 (stage, then placement index). Stored tables are zero
    u16 state;        // Bits 0..1 initial flag. Bit 9 is AREA_OBJECT_PLACE_PROMPT. Bit 8 is unread
    s16 x;            // World X, in game-coordinate units
    s16 y;            // World Y, in game-coordinate units
    s16 z;            // World Z, in game-coordinate units
    u16 yaw;          // Yaw, 0x1000 units per turn. Zero leaves the rotation unapplied
} AreaObjectPlace;
STATIC_ASSERT_SIZEOF(AreaObjectPlace, 0x10);

/// `kind` value that ends an `AreaObjectSpawn` table.
enum { AREA_OBJECT_SPAWN_END = 0xFFFF };

/// How a room spawns the placed objects of one kind.
///
/// A room's table of these, ended by `AREA_OBJECT_SPAWN_END`, sits beside its
/// `AreaObjectPlace` list. Each place whose flag is set spawns from the first
/// entry whose `kind` equals the place's `kind`; a kind with no entry spawns
/// nothing. Entries cover enemies, pickups and room props alike - every spawn
/// is given `Enemy` work, placed at the record's position and yaw when its
/// body is not `TASK_BODY_NONE`.
typedef struct {
    u16      kind;     // `AreaObjectPlace.kind` this entry spawns; also the task's spawn argument 1
    TaskDesc taskDesc; // Spawn recipe: body, priority, per-frame callback and model
} AreaObjectSpawn;
STATIC_ASSERT_SIZEOF(AreaObjectSpawn, 0x10);

/// Value of `AreaObjectRoom.places.sentinel` that ends a stage's room table.
///
/// Walks that seed an area's 2-bit flags stop on this entry and do not read
/// it. Every stored table ends with it, and `spawns` is NULL there.
enum { AREA_OBJECT_ROOM_END = -1 };

/// Value of `AreaObjectRoom.places.sentinel` that ends a search for one flag
/// index across a stage's rooms.
///
/// Stored tables end with `AREA_OBJECT_ROOM_END`. A search that misses every
/// room continues past that entry until this value.
enum { AREA_OBJECT_ROOM_LOOKUP_END = 0x7FFFFFFF };

/// One area's placed objects: its place list and the spawns those places use.
///
/// Spawning reads the entry at `GameLocationKey.area`. Flag seeding and a
/// search for one flag index walk from the first entry. `places.list` is that
/// area's `AreaObjectPlace` list, or NULL when the area has none. `spawns` is
/// the `AreaObjectSpawn` table those places are created from, or NULL. The
/// last entry stores `AREA_OBJECT_ROOM_END`. A flag-index search stops when
/// the same word equals `AREA_OBJECT_ROOM_LOOKUP_END`.
typedef struct {
    union {
        AreaObjectPlace* list;     // This area's place list. NULL when the area has none
        s32              sentinel; // AREA_OBJECT_ROOM_END ends flag seeding. AREA_OBJECT_ROOM_LOOKUP_END ends a flag-index search
    } places;
    AreaObjectSpawn* spawns;       // This area's spawn table, or NULL
} AreaObjectRoom;
STATIC_ASSERT_SIZEOF(AreaObjectRoom, 0x8);

/// `stage` value that ends an `AreaApplyRec` list.
///
/// The entry's other bytes are zero and are not read.
enum { AREA_APPLY_END = 0xFF };

/// High nibble of `AreaApplyRec.policy`: which save modes receive the update.
///
/// Compared with `McSaveState.gameMode` (0 normal/replay, 1 Bounty, 2 Scavenger,
/// 3 Nightmare). Any other value skips the entry. Stored lists use only these three.
enum {
    AREA_APPLY_MODE_MASK             = 0xF0,
    AREA_APPLY_MODE_ALWAYS           = 0x00, // Every mode
    AREA_APPLY_MODE_REPLAY_SCAVENGER = 0x10, // Modes 0 and 2
    AREA_APPLY_MODE_BOUNTY_NIGHTMARE = 0x20  // Modes 1 and 3
};

/// Low nibble of `AreaApplyRec.policy`.
///
/// Nonzero sets `AREA_SAVED_MAP_MARK`; zero clears it. The mark changes only
/// when the mode nibble accepts the entry. Stored lists use 0 and 1.
enum { AREA_APPLY_MAP_MARK_MASK = 0x0F };

/// One saved-area update in a list ended by `AREA_APPLY_END`.
///
/// When the area has saved state, an accepted entry stores `variant` as
/// its placement layout, removes that area's saved enemy poses, and then
/// sets or clears its map mark. `stage` and `area` select the saved area
/// the same way as `GameLocationKey`. An area with no saved state is left
/// unchanged. A list may name one area twice, with a different layout for
/// each save-mode group.
typedef struct {
    u8 stage;   // Stage id (1..5, as GameLocationKey.stage). AREA_APPLY_END ends the list
    u8 area;    // 1-based area within that stage, as GameLocationKey.area
    u8 variant; // Placement layout stored for the area
    u8 policy;  // High nibble AREA_APPLY_MODE_*; low nibble AREA_APPLY_MAP_MARK_MASK
} AreaApplyRec;
STATIC_ASSERT_SIZEOF(AreaApplyRec, 4);

#endif // GAMEPLAY_AREA_FLAGS_H
