#ifndef MAIN_GAMEFLAG_TYPES_H
#define MAIN_GAMEFLAG_TYPES_H

#include "common.h"

/// Number of four-bit positions in a saved game-flag payload.
enum { GAME_FLAG_NIBBLE_COUNT = 504 };

/// Indices and extent of the resident packed game-flag save bank pair.
enum {
    /// Index of the current session's bank in `gGameFlagNibbleBanks`.
    ///
    /// Holds the live packed flags and their shared play-time mark. Saving
    /// snapshots this bank into `GAME_FLAG_NIBBLE_BANK_BACKUP`; demo restore
    /// replaces only this bank.
    GAME_FLAG_NIBBLE_BANK_LIVE   = 0,
    GAME_FLAG_NIBBLE_BANK_BACKUP = 1,
    GAME_FLAG_NIBBLE_BANK_COUNT  = 2
};

/// Checksummed save bank of packed game flags and a play-time mark.
///
/// Each 0x100-byte bank has a four-byte checksum header and 252 payload bytes.
/// Nibble indices 0..503 select the high nibble for even indices and the low
/// nibble for odd indices. The minute mark shares nibble positions 104..107;
/// these views refer to the same storage. Whole-byte accesses update both
/// nibbles together. The memory-card system stores the live bank followed by
/// its backup and computes their checksums when saving.
typedef struct {
    u16 checksum;                                   // Low 16 bits of the sum of all payload bytes interpreted as s8
    u16 checksumComplement;                         // Ones' complement of checksum; verification compares only checksum
    union {
        u8 packedFlags[GAME_FLAG_NIBBLE_COUNT / 2]; // Two four-bit values per byte
        struct {
            u8  flagBytesBeforeTimeMark[0x34];      // Packed nibble positions 0..103
            u16 playTimeMark;                       // Captured play time in minutes (0..59999), refreshed for collected-bit timing
            u8  flagBytesAfterTimeMark[0xC6];       // Packed nibble positions 108..503
        } state;                                    // Wider gameplay interpretation of the packed payload
    } payload;                                      // Saved bytes covered by the checksum
} GameFlagNibbleBank;
STATIC_ASSERT_SIZEOF(GameFlagNibbleBank, 0x100);
STATIC_ASSERT(OFFSET_OF(GameFlagNibbleBank, payload) == 4, game_flag_payload_offset);
STATIC_ASSERT(OFFSET_OF(GameFlagNibbleBank, payload.state.playTimeMark) == 0x38, game_flag_play_time_offset);

/// Common saved stage header. Area IDs 1..64 select visitedAreas; each
/// entryStates word holds sixteen two-bit placement/item states (IDs 0..63).
typedef struct _GpFlagBank {
    u16 checksum;
    u16 checksumComplement;
    s32 visitedAreas[2];
    u32 entryStates[4];
    u8  unknown_1C[4]; // Saved bytes before the first referenced area record
} GpFlagBank;
STATIC_ASSERT_SIZEOF(GpFlagBank, 0x20);
STATIC_ASSERT(OFFSET_OF(GpFlagBank, entryStates) == 0xC, game_flag_entry_states_offset);

/// Default placement layout and bits of `AreaSavedState.spawnFlags`.
enum {
    AREA_DEFAULT_VARIANT           = 1,
    AREA_SPAWN_RESET_SAVED_POSES   = 0x01,
    AREA_SPAWN_RESTORE_SAVED_POSES = 0x02,
    /// Map mark in `AreaSavedState.spawnFlags`.
    ///
    /// When `AREA_SPAWN_RESTORE_SAVED_POSES` is clear, the world map draws a
    /// visited area's room in red and the shelter map draws a marker. The bit is
    /// set when an area-exit sequence starts, for the new-game area lists, and
    /// when an area-apply record's low nibble is nonzero. It is cleared when
    /// that nibble is zero, when a generator's death release runs, and at the
    /// end of the driveway cutscene.
    AREA_SAVED_MAP_MARK = 0x04
};

/// Saved placement variant and flags for one area.
///
/// The live prefix of a `GameFlagAreaSlot`. Stage area tables point at these
/// bytes in the stage save bank, so spawn and map code update the save directly.
/// A zero `variant` is uninitialized; the next spawn preparation or location
/// sync stores `AREA_DEFAULT_VARIANT` and requests a saved-pose reset.
typedef struct {
    s8 variant;    // Placement layout (0 uninitialized, then AREA_DEFAULT_VARIANT). Copied into `GameLocationKey.variant`
    u8 spawnFlags; // AREA_SPAWN_RESET_SAVED_POSES, AREA_SPAWN_RESTORE_SAVED_POSES, AREA_SAVED_MAP_MARK
} AreaSavedState;
STATIC_ASSERT_SIZEOF(AreaSavedState, 2);

/// Area records are saved at four-byte intervals. The remaining two bytes
/// are part of the saved bank; their role has not been established.
typedef struct {
    AreaSavedState state;
    u8             unknown_2[2];
} GameFlagAreaSlot;
STATIC_ASSERT_SIZEOF(GameFlagAreaSlot, 4);

typedef struct {
    GpFlagBank       header;
    GameFlagAreaSlot areas[18];
    u8               unknown_68[4];
} GameFlagAcropolisBank;
STATIC_ASSERT_SIZEOF(GameFlagAcropolisBank, 0x6C);

typedef struct {
    GpFlagBank       header;
    GameFlagAreaSlot areas[36];
} GameFlagDryfieldBank;
STATIC_ASSERT_SIZEOF(GameFlagDryfieldBank, 0xB0);

typedef struct {
    GpFlagBank header;
    u8         unknown_20[4];
} GameFlagDryfieldFullBank;
STATIC_ASSERT_SIZEOF(GameFlagDryfieldFullBank, 0x24);

typedef struct {
    GpFlagBank       header;
    GameFlagAreaSlot areas[48];
    u8               unknown_E0[4];
} GameFlagShelterBank;
STATIC_ASSERT_SIZEOF(GameFlagShelterBank, 0xE4);

typedef struct {
    GpFlagBank       header;
    GameFlagAreaSlot areas[32];
    u8               unknown_A0[4];
} GameFlagNeoArkBank;
STATIC_ASSERT_SIZEOF(GameFlagNeoArkBank, 0xA4);

#endif // MAIN_GAMEFLAG_TYPES_H
