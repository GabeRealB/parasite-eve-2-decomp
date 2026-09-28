#ifndef MAIN_MC_TYPES_H
#define MAIN_MC_TYPES_H

#include <psyq/sys/types.h>

#include "common.h"

#include "main/session_types.h"

/// One row of an item table: the item it holds, the attachment slot that item
/// occupies and its stack count. The tables a `McItemScan` chooses between
/// (`Mc_SaveData[0].state.itemRows`, `Gp_ItemTable1`, `Gp_ItemTable2`) are arrays of
/// these rows. A non-zero `attachSlot` marks the row as in use: 1..n is the
/// slot the item occupies in the equipped weapon's or armour's attachment list,
/// and -1 marks the row whose item is the equipped armour itself.
typedef struct {
    u8  itemId;     // Item id; 0 marks the row free
    s8  attachSlot; // Attachment slot the item occupies (-1 = the equipped armour itself)
    u16 qty;        // Stack count
} McItemRec;
STATIC_ASSERT_SIZEOF(McItemRec, 0x4);

/// One entry of `Mc_SaveData`'s per-weapon item table, reached through
/// `Gp_GetItemSlot` and indexed by weapon item id (0x80–0x9F): the ammunition
/// the weapon is loaded with, and the attachment fitted to it.
typedef struct {
    u8  ammoId;    // Ammunition item id (0 = none loaded)
    u8  ammoQty;   // How many of ammoId the weapon holds
    u8  attachId;  // Attachment item id (0xFF = the weapon takes none, 0 = slot empty)
    u8  attachQty; // How many of attachId the weapon holds
    s32 field_4;   // Role unproven: cleared wherever a slot is reset, and no body reads it
} McItemSlot;
STATIC_ASSERT_SIZEOF(McItemSlot, 0x8);

/// Window on the rows of an item table an inventory operation works on: which
/// table, the first row and how many rows. The tables it selects between hold
/// `McItemRec` rows, and each place the game stores items keeps its own window.
/// `Mc_SaveData[0].state.carriedItems` holds the window on the items the player carries, so
/// the menus act on that window instead of on a whole table; a scan can also be
/// built locally to search a wider run of rows.
typedef struct {
    u8 firstRow; // First row of the window
    u8 rowCount; // Number of rows the window covers
    u8 table;    // Table the window lies in (0 `Mc_SaveData[0].state.itemRows`, 1 `Gp_ItemTable1`, 2 `Gp_ItemTable2`)
    u8 field_3;  // Nothing reads or writes it; role unproven
} McItemScan;
STATIC_ASSERT_SIZEOF(McItemScan, 0x4);

/// One saved pose of a placed enemy: the position and rotation it had when it
/// was taken out of the world, and the state it is to be resumed in.
/// `Mc_SaveData[0].state.enemyPoses` holds a fixed run of these, filed by
/// `Gp_SaveEnemyPose` and read back by `Gp_SpawnArea`, so an area the player
/// returns to puts the enemy back where they left it - and an enemy the table
/// holds no record for is not put back at all.
///
/// A record is keyed by `placeKey`, the same key the enemy itself carries, and
/// `spawnState` doubles as the slot's occupancy marker, 0 meaning free. The
/// rotation shares the record with the position, so each angle is kept as the
/// high byte of the angle it was taken from, and shifted back up on restore.
typedef struct {
    u8  pitch;      // Rotation about X, kept as the angle's high byte
    u8  yaw;        // Rotation about Y, likewise
    u8  roll;       // Rotation about Z, likewise
    s8  spawnState; // State the enemy is resumed in; 0 marks the slot free
    s16 x;          // X of the enemy's coordinate, narrowed to 16 bits
    s16 y;          // Y of the enemy's coordinate, likewise
    s16 z;          // Z of the enemy's coordinate, likewise
    u16 placeKey;   // Placement the enemy was spawned from, as `GpEnemy.placeKey`
} McPosRec;
STATIC_ASSERT_SIZEOF(McPosRec, 0xC);

/// The save data: everything a memory card save holds, resident in main's BSS
/// and copied out as one image.
///
/// It carries the run's progress - where the player was, the time played, the
/// player's statistics and the whole inventory with its per-weapon equipment -
/// and the record of what the game has already done: which stages and areas
/// were visited, which placed enemies were taken out of the world and where
/// they were left, which items have been seen, and what the shops still hold in
/// stock. The header block and the data block each carry a checksum pair.
typedef struct {
    byte       unknown_0[0x4];
    GameLoc    at4;               // Place the save was made at, as `GameSession.at4`
    u16        playTime;          // Played time in minutes (capped at 0xEA5F)
    s8         clearCount;        // Times the game has been completed (0 never, capped at 99)
    s8         gameMode;          // Mode the run is played in (0..2); the stat, cost and item-grant tables have one variant per mode
    u8         visitFlags;        // One bit per stage marking it as visited; bit 0 marks the new-game setup as done
    s8         saveNumber;        // Number of this save among those made at the same save point (1..99)
    u8         savePoint;         // Save point the save was made at, indexing the tables of place names the slot and the save header print (1..16)
    s8         companionType;     // Companion the save carries (0 none, otherwise 1..3 index `Gp_AllyIdBase`)
    s32        playerExp;         // Player experience as of the save, as `Player_Status.exp`
    s32        playerBp;          // Player BP as of the save, as `Player_Status.bp`
    u16        hdrChecksum;       // Sum over the header's 0x38 bytes, which start at `at4`
    u16        hdrChecksumInv;    // Complement of `hdrChecksum`, written with it
    byte       unknown_20[0x1];
    s8         vibration;         // Vibration setting (0 on, 1 off)
    s8         characterId;       // Character the save plays as (1-based); the weapon, animation and placement tables are indexed by it
    s8         demoScene;         // Attract demo being played back (0 during normal play)
    byte       unknown_24[0x1];
    u8         moveMode;          // Default movement (0 walk, 1 run)
    u8         hpBonus;           // Addend to the player's maximum HP
    u8         mpBonus;           // Addend to the player's maximum MP
    McPosRec   enemyPoses[0x20];  // The placed enemies taken out of the world, filed by their placement key
    s8         buttonLayout;      // Button layout the pad is remapped through (0..2)
    s8         soundMode;         // Sound output (0 stereo, 1 mono)
    s8         musicVolume;       // Music volume (0..3, 3 is off)
    s8         cursorMode;        // Cursor behaviour in the menus (0 remembers the row, 1 resets it)
    McItemRec  itemRows[0x100];   // The save's own item table, indexed by row; the rows the player carries are `carriedItems`
    s32        collectedBits[4];  // 128 bits, one per collectible the player has picked up
    McItemScan carriedItems;      // Window on the player's rows of `itemRows`
    u8         unknown_5C0;
    u8         field_5C1;         // Role unproven: while it reads 1 the actors stop moving and stop pushing each other apart
    s8         cheatMode;         // While set nothing is spent - no MP for the abilities, no ammunition and no attachments - and the save is left out of the save-slot comparison
    s8         interlace;         // Non-zero runs the display interlaced
    byte       unknown_5C4;
    s8         sceneEvent;        // Scene event a script arms for the music and sound task
    byte       unknown_5C6[0x1];
    s8         companionVariant;  // Which variant of `companionType` is spawned
    McItemSlot weaponItems[0x20]; // Per-weapon equipment, indexed by item id - 0x80: the ammunition loaded and the attachment fitted
    s16        companionHp;       // Companion's current HP
    s16        companionHpMax;    // Companion's maximum HP
    u16        field_6CC;
    u16        field_6CE;
    s32        itemSeenBits[0x60];    // One bit per item id (ids at or above 0x180 count as seen), set once the item was looked at
    u8         attachLevels[0x12];    // Level bought for each Parasite Energy slot (0..3), addressed as page * 3 + column
    s16        attachUseCounts[19];   // Times each Parasite Energy slot was used; the Play Data panel reports the first twelve
    s32        weaponUseCounts[0x20]; // Times each weapon was used, indexed by item id - 0x80
    s8         itemLevelBonus[0x20];  // Addend to the level of items 0x60-0x7F
    byte       unknown_928[0x1];
    s8         field_929;
    s8         replayRank; // Rank the replay earns (0 none, 1, 2)
    u8         saveCount;  // How many times the game has been saved (capped at 99)
    s32        field_92C;
    s32        field_930;
    s32        shopTiers;         // Bitmask of the 13 price tiers the parking-lot shop offers
    u32        shopStock;         // 12 two-bit stock levels for that shop
    u16        dataChecksum;      // Sum over the save's data block
    u16        dataChecksumInv;   // Complement of `dataChecksum`, written with it
    u16        bufferChecksum;    // Sum over the first byte of each memcard buffer slot
    u16        bufferChecksumInv; // Complement of `bufferChecksum`, written with it
} McSaveState;

/// Exact 128-byte save prefix read for a memory-card slot preview.
/// The trailing bytes are retained for the header checksum and are not
/// interpreted as the remainder of the full save.
typedef struct {
    byte    unknown_0[0x4];
    GameLoc at4;            // Place the save was made at, as `GameSession.at4`
    u16     playTime;       // Played time in minutes (capped at 0xEA5F)
    s8      clearCount;     // Times the game has been completed (0 never, capped at 99)
    s8      gameMode;       // Mode the run is played in (0..2); the stat, cost and item-grant tables have one variant per mode
    u8      visitFlags;     // One bit per stage marking it as visited; bit 0 marks the new-game setup as done
    s8      saveNumber;     // Number of this save among those made at the same save point (1..99)
    u8      savePoint;      // Save point the save was made at, indexing the tables of place names the slot and the save header print (1..16)
    s8      companionType;  // Companion the save carries (0 none, otherwise 1..3 index `Gp_AllyIdBase`)
    s32     playerExp;      // Player experience as of the save, as `Player_Status.exp`
    s32     playerBp;       // Player BP as of the save, as `Player_Status.bp`
    u16     hdrChecksum;    // Sum over the header's 0x38 bytes, which start at `at4`
    u16     hdrChecksumInv; // Complement of `hdrChecksum`, written with it
    u8      remainingBytes[0x60];
} McSavePreview;
STATIC_ASSERT_SIZEOF(McSavePreview, 0x80);

/// Full save image and its bounded cached-preview representation.
/// Both records have the same initial fields through the checksum pair.
typedef union {
    McSaveState   state;
    McSavePreview preview;
} McSaveData;
STATIC_ASSERT_SIZEOF(McSaveData, 0x944);
STATIC_ASSERT(OFFSET_OF(McSaveData, state.at4) == 4, McSaveData_at4);
STATIC_ASSERT(OFFSET_OF(McSaveData, state.playTime) == 0xC, McSaveData_playTime);

#endif // MAIN_MC_TYPES_H
