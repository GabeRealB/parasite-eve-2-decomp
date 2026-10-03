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

/// Saved prefix of a stage bank.
///
/// The memory-card system stores each stage bank as one checksummed block.
/// The sum covers every byte after the first four, including area records
/// that follow this prefix in the larger banks. Loading compares that sum
/// and ignores its complement.
typedef struct {
    u16 checksum;           // Low 16 bits of the signed byte sum of the stage bank after these four bytes
    u16 checksumComplement; // Ones' complement of checksum. Loading compares checksum only
    s32 visitedAreas[2];    // Ids 1..32, then 33..64, one bit each. Set when the area is marked visited; both words are cleared on the stage's first entry
    u32 objectStates[4];    // Ids 0..63, sixteen two-bit states per word, low pair first. Seeded from the place list on first entry. Night Dryfield uses the daytime bank's words and is not reseeded
    u8  unknown_1C[4];      // Saved with the bank. No access through this prefix has been found; role unproven
} GameFlagStageHeader;
STATIC_ASSERT_SIZEOF(GameFlagStageHeader, 0x20);
STATIC_ASSERT(OFFSET_OF(GameFlagStageHeader, objectStates) == 0xC, game_flag_object_states_offset);

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
    /// when an `AreaApplyRec` policy's low nibble is nonzero. It is cleared when
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

/// Saved record for one area in a stage bank.
///
/// Stage area tables address `state`, and placement and map updates go
/// through that pointer. Records are four bytes apart. The two bytes after
/// `state` are stored in the bank and covered by its checksum. Nothing reads
/// or writes those bytes on their own, so their role is unproven.
typedef struct {
    AreaSavedState state;        // Live placement variant and spawn flags
    u8             unknown_2[2]; // Stored with the bank and covered by its checksum. No field-level access found; role unproven
} GameFlagAreaSlot;
STATIC_ASSERT_SIZEOF(GameFlagAreaSlot, 4);

/// Bytes in one Akropolis stage-bank copy, checksum prefix included.
///
/// `GameFlag_AcropolisBanks` stores the live copy followed by its memory-card
/// backup. The pair occupies two 128-byte card sectors.
enum { GAME_FLAG_ACROPOLIS_BANK_BYTES = 0x6C };

/// Checksummed save bank for `GAME_STAGE_ACROPOLIS`.
///
/// That stage is Akropolis Tower together with the MIST areas reached from
/// it. The live header is the `Gp_FlagBanks` entry for the stage, and the
/// Akropolis map reads this bank's object-state words. The stage area table
/// addresses `areas`: a slot's index is the area id minus one up to
/// `GAME_AREA_ACROPOLIS_HELICOPTER_LANDING_PAD`, and the area id minus two for
/// `GAME_AREA_MIST_R18` and `GAME_AREA_MIST_PARKING`.
/// `GAME_AREA_ACROPOLIS_WEST_ELEVATOR_HALL` has an empty table row and no slot,
/// and `GAME_AREA_MIST_SHOOTING_GALLERY`'s row addresses slot 29 of the
/// Dryfield bank instead. The highest addressed slot is 17.
typedef struct {
    GameFlagStageHeader header;        // Visited-area bits and object states for Akropolis Tower and the MIST
    GameFlagAreaSlot    areas[18];     // Placement records. Slots 0..15 are the tower's areas 1..16; slots 16 and 17 are MIST areas 18 and 19
    u8                  unknown_68[4]; // Stored with the bank and covered by its checksum. No field-level access found; role unproven
} GameFlagAcropolisBank;
STATIC_ASSERT_SIZEOF(GameFlagAcropolisBank, GAME_FLAG_ACROPOLIS_BANK_BYTES);

/// Bytes in one Dryfield day-bank copy, checksum prefix included.
///
/// `GameFlag_DryfieldBanks` stores the live copy followed by its memory-card
/// backup. The pair occupies three 128-byte card sectors.
enum { GAME_FLAG_DRYFIELD_BANK_BYTES = 0xB0 };

/// Checksummed save bank for Dryfield by day (`GAME_STAGE_DRYFIELD`).
///
/// The live header is the `Gp_FlagBanks` entry for that stage. Dryfield by
/// night (`GAME_STAGE_DRYFIELD_NIGHT`) keeps its own stage header in
/// `GameFlagDryfieldNightBank`. Night area tables and night object-state words
/// address this live bank. Where the day and night tables both have a record
/// for one area id, both records address the same slot. The slot index is
/// independent of the area id. Slot 29 is the MIST shooting gallery record
/// from the Akropolis stage table. Slots 30..32 have no table entry; they are
/// stored with the bank and covered by its checksum. The highest addressed
/// slot is 35.
typedef struct {
    GameFlagStageHeader header;    // Visited-area bits and object states for Dryfield by day. Night reads these object states
    GameFlagAreaSlot    areas[36]; // Day and night placement records. Slot 29 is the MIST shooting gallery; slots 30..32 have no table entry
} GameFlagDryfieldBank;
STATIC_ASSERT_SIZEOF(GameFlagDryfieldBank, GAME_FLAG_DRYFIELD_BANK_BYTES);

/// Bytes in one Dryfield night-bank copy, checksum prefix included.
///
/// `GameFlag_DryfieldFullBanks` stores the live copy followed by its
/// memory-card backup. The pair fits one 128-byte card sector.
enum { GAME_FLAG_DRYFIELD_NIGHT_BANK_BYTES = 0x24 };

/// Checksummed save bank for Dryfield by night (`GAME_STAGE_DRYFIELD_NIGHT`).
///
/// The live header is the `Gp_FlagBanks` entry for that stage, so night areas
/// set their visited bits here; the night map also shows areas visited by day.
/// The bank holds no area records. Night area tables and night object-state
/// words address `GameFlagDryfieldBank`, so this header's object-state words
/// are stored but not used.
typedef struct {
    GameFlagStageHeader header;        // Visited-area bits for Dryfield by night. Its object-state words are unused
    u8                  unknown_20[4]; // Stored with the bank and covered by its checksum. No field-level access found; role unproven
} GameFlagDryfieldNightBank;
STATIC_ASSERT_SIZEOF(GameFlagDryfieldNightBank, GAME_FLAG_DRYFIELD_NIGHT_BANK_BYTES);

typedef struct {
    GameFlagStageHeader header;
    GameFlagAreaSlot    areas[48];
    u8                  unknown_E0[4];
} GameFlagShelterBank;
STATIC_ASSERT_SIZEOF(GameFlagShelterBank, 0xE4);

/// Bytes in one Shelter and Neo Ark stage-bank copy, checksum prefix included.
///
/// `GameFlag_NeoArkBanks` stores the live copy followed by its memory-card
/// backup. The pair occupies three 128-byte card sectors.
enum { GAME_FLAG_NEO_ARK_BANK_BYTES = 0xA4 };

/// Checksummed save bank for `GAME_STAGE_SHELTER_NEO_ARK`.
///
/// That stage is the Shelter's 1F and B6 together with the Neo Ark. The live
/// header is the `Gp_FlagBanks` entry for the stage, and the Neo Ark map reads
/// this bank's object-state words. The stage area table addresses `areas`. A
/// slot's index is the area id minus one, except
/// `GAME_AREA_SHELTER_B6_GROWTH_ROOM`, which shares the nursery's slot. Slots
/// 5, 8 and 19 are the unused indexes of the empty guardroom, Eve-elevator and
/// altar rows. Slot 22 is the growth room's unused index. Those four slots are
/// stored with the bank and covered by its checksum. The highest addressed
/// slot is 31. `GAME_AREA_NEO_ARK_SUBSTATION` has an empty table row and no
/// slot.
typedef struct {
    GameFlagStageHeader header;        // Visited-area bits and object states for the Shelter's 1F and B6 and the Neo Ark
    GameFlagAreaSlot    areas[32];     // Placement records. Slots 5, 8, 19 and 22 have no table entry; slot 21 is shared by the nursery and the growth room
    u8                  unknown_A0[4]; // Stored with the bank and covered by its checksum. No field-level access found; role unproven
} GameFlagNeoArkBank;
STATIC_ASSERT_SIZEOF(GameFlagNeoArkBank, GAME_FLAG_NEO_ARK_BANK_BYTES);

#endif // MAIN_GAMEFLAG_TYPES_H
