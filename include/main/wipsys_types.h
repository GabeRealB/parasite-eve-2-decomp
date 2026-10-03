#ifndef MAIN_WIPSYS_TYPES_H
#define MAIN_WIPSYS_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// Game disc identified in the drive, by which stages' `.CDF` files it carries.
///
/// Disc 1 holds stages 1 and 2, disc 2 stages 4 and 5.
enum {
    GAME_MAIN_DISC_UNKNOWN = 0,
    GAME_MAIN_DISC_1       = 1,
    GAME_MAIN_DISC_2       = 2,
};

/// System state cleared once at power-on and kept across the game's soft resets.
///
/// Unlike the session, which each new game or reset clears, these flags carry
/// knowledge from one boot cycle to the next: which disc is inserted, whether
/// the title intro has already played, and whether a game over has occurred.
/// It also records whether the CD stream ring and MDEC decoder are installed, so
/// a CD reset can tear them down.
typedef struct {
    byte discNumber;        // Inserted disc, GAME_MAIN_DISC_* (0 unknown, 1 disc 1, 2 disc 2); rewritten by each ISO directory scan
    s8   gameOver;          // Set by a game over, never cleared (0 no, 1 yes); the title menu then opens on Load Game
    byte unknown_2[2];      // No observed accesses
    s16  skipTitleIntro;    // Next title boot skips the logo intro (0 play it, 1 skip it); an idle title menu and a demo played to its end clear it
    s16  movieStreamActive; // Stream ring and MDEC decoder are installed for movie playback (0 no, 1 yes)
    byte unknown_8[0x18];   // No observed accesses; extent taken from the symbol size
} GameMainPersistentState;
STATIC_ASSERT_SIZEOF(GameMainPersistentState, 0x20);

/// Captured player position and facing for restoring the player at room entry.
///
/// `PlayerStatus` retains this snapshot independently of the live actor and
/// includes it in memory-card saves. Coordinates use integer world units:
/// capture keeps their low 16 bits, and restore sign-extends them to 32 bits.
/// Yaw is taken from the root matrix's forward direction, with both half-turn
/// endpoints retained in the range [-2048, 2048].
typedef struct {
    s16 x;   // Captured root X position, in signed world units
    s16 y;   // Captured root Y position, in signed world units
    s16 z;   // Captured root Z position, in signed world units
    s16 yaw; // Captured facing about Y, 4096 units per turn, inclusive [-2048, 2048]
} PlayerPos;
STATIC_ASSERT_SIZEOF(PlayerPos, 0x8);

/// Serialized byte length of each player-state copy, including its checksum header.
enum { PLAYER_STATUS_SAVE_RECORD_BYTES = 0x40 };

/// Upper limit applied when recalculating maximum player HP and MP.
enum { PLAYER_STATUS_STAT_MAX = 250 };

/// Empty selection in the compact weapon, primary-item and armour fields.
enum { PLAYER_STATUS_EQUIPMENT_NONE = 0 };

/// Timed player status-effect masks stored in `PlayerStatus::statusFlags`.
///
/// Bit 0x08 is not retained as a timed effect. Bit 0x20 has a countdown and
/// a HUD icon, but its gameplay meaning is unproven.
enum {
    PLAYER_STATUS_DARKNESS    = 0x01,
    PLAYER_STATUS_PARALYSIS   = 0x02,
    PLAYER_STATUS_POISON      = 0x04,
    PLAYER_STATUS_SILENCE     = 0x10,
    PLAYER_STATUS_CONFUSION   = 0x40,
    PLAYER_STATUS_BERSERKER   = 0x80,
    PLAYER_STATUS_ALL_EFFECTS = 0xFF,
};

/// Resident live player state followed by its serialized memory-card backup.
///
/// Save/load checksums, copies and compares two `PLAYER_STATUS_SAVE_RECORD_BYTES`
/// byte images. The live image includes a borrowed actor matrix pointer;
/// loading the bytes does not restore that pointer's lifetime. Player actor
/// initialization binds it to the new actor before gameplay uses it.
/// The backup is an uninterpreted byte image, initially filled with 0xFF.
typedef struct {
    u16       checksum;                                    // Sum of signed payload bytes, modulo 65536, excluding the four-byte header
    u16       checksumComplement;                          // Bitwise complement of checksum
    MATRIX*   coordMtx;                                    // Borrowed root coordinate matrix; valid while the player actor is live
    s32       exp;                                         // Unspent experience used to unlock and raise Parasite Energy levels
    s32       bp;                                          // Bounty points available to spend in shops
    PlayerPos pos;                                         // Captured room-entry position and facing, independent of the live matrix
    s16       hp;                                          // Current HP; damage can leave it zero or negative
    s16       hpMax;                                       // Maximum HP after mode base, permanent bonus and armour; capped at PLAYER_STATUS_STAT_MAX
    s16       mp;                                          // Current Parasite Energy points
    s16       mpMax;                                       // Maximum MP after PE levels, mode base, permanent bonus and armour; capped at PLAYER_STATUS_STAT_MAX
    u8        field_20;                                    // Initialization writes zero; role unproven
    u8        weapon;                                      // Equipped weapon itemId - 0x7F (0 none, 1..32 weapon)
    u8        weaponSlotItem;                              // Primary weapon-slot itemId - 0x9F, stored modulo 256 (0 empty)
    u8        armor;                                       // Equipped armour itemId - 0x5F (0 none, 1..32 armour)
    u8        interactionPressed;                          // Accepted interaction button press (0 no, 1 yes); scene movement clears it
    u8        statusFlags;                                 // Timed effects using PLAYER_STATUS_* masks; effect 0x20 remains unidentified
    u8        resourceVariant;                             // One-based model/texture/file set; player paths write 1..4, companion setup temporarily substitutes its variant
    byte      unknown_27[0x19];                            // Included in save/checksum operations; individual roles unproven
    u8        saveBackup[PLAYER_STATUS_SAVE_RECORD_BYTES]; // Serialized backup of the entire live image, including its checksum header
} PlayerStatus;
STATIC_ASSERT_SIZEOF(PlayerStatus, 0x80);
STATIC_ASSERT(OFFSET_OF(PlayerStatus, saveBackup) == PLAYER_STATUS_SAVE_RECORD_BYTES, player_status_backup_offset);

#endif // MAIN_WIPSYS_TYPES_H
