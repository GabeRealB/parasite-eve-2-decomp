#ifndef MAIN_GAMEFLAG_TYPES_H
#define MAIN_GAMEFLAG_TYPES_H

#include "common.h"

/// One checksummed bank with 504 packed four-bit flag slots. Gameplay also
/// views some slots as wider state fields. The memory-card system stores the
/// current bank followed by its backup, 0x100 bytes each.
typedef struct {
    u16 checksum;
    u16 checksumComplement;
    union {
        u8 values[0xFC];
        struct {
            u8  unknown_04[0x34];
            u16 playTimeMark; // Saved play time when collected flag 0x119 was set
            u8  unknown_3A[0xC6];
        } state;
    } data;
} GameFlagNibbleBank;
STATIC_ASSERT_SIZEOF(GameFlagNibbleBank, 0x100);
STATIC_ASSERT(OFFSET_OF(GameFlagNibbleBank, data.state.playTimeMark) == 0x38, game_flag_play_time_offset);

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

/// Default placement layout and controls for saved enemy poses.
enum {
    AREA_DEFAULT_VARIANT           = 1,
    AREA_SPAWN_RESET_SAVED_POSES   = 0x01,
    AREA_SPAWN_RESTORE_SAVED_POSES = 0x02
};

/// Saved placement variant and spawn flags for one area.
typedef struct {
    s8 variant;    // Placement/resource layout (0 uninitialized, 1 default); copied into `GameLocationKey.variant`
    u8 spawnFlags; // Bit 0 discard saved enemy poses, bit 1 restore them, bit 2 used by the area status lists
} GpAreaObj;
STATIC_ASSERT_SIZEOF(GpAreaObj, 2);

/// Area records are saved at four-byte intervals. The remaining two bytes
/// are part of the saved bank; their role has not been established.
typedef struct {
    GpAreaObj state;
    u8        unknown_2[2];
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
