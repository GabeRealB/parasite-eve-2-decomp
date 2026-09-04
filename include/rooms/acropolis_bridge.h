#ifndef ROOMS_ACROPOLIS_BRIDGE_H
#define ROOMS_ACROPOLIS_BRIDGE_H

#include "common.h"

/// Work block this room's script tasks keep at `Task::idMap`
/// (`Mem_Calloc(0x10, 0)` in `func_acropolis_bridge_8017E04C`). `field_4` is
/// the script step handed to `func_acropolis_bridge_8017E60C` and
/// `promptKind` the display mode forwarded to `func_800D4E78`.
typedef struct AcropolisBridgePromptWork {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s16  field_4;
    /* 0x06 */ s8   field_6;
    /* 0x07 */ u8   retryCount;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ byte pad_C[0x2];
    /* 0x0E */ s8   promptKind;
    /* 0x0F */ s8   promptBusy;
} AcropolisBridgePromptWork;

/// Payload this room passes as `Gp_DispatchMsg`'s `arg2` for message 0x7DA,
/// sent by both `func_acropolis_bridge_8017DC68` and
/// `func_acropolis_bridge_8017F544`. It is forwarded unchanged to the 0x7DB
/// handlers, which read the two id bytes back as one halfword; see
/// `AcropolisBridgeMsg7DB`.
typedef struct AcropolisBridgeMsg7DA {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} AcropolisBridgeMsg7DA;

/// The same four bytes as `AcropolisBridgeMsg7DA`, as the bridge enemy's 0x7DB
/// handler reads them. `func_acropolis_bridge_801856E0` compares the id bytes
/// as a single halfword (0x0B01 for the lower request, 0x0E01 for the end of
/// the run) instead of testing `field_0` and `field_1` separately.
typedef struct AcropolisBridgeMsg7DB {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} AcropolisBridgeMsg7DB;

/// One entry of the bridge's -1-terminated hotspot table
/// (`D_acropolis_bridge_8018983C`). `x` / `y` / `w` / `h` are the screen
/// rectangle `func_acropolis_bridge_8017F6D4` tests the action cursor against;
/// on a hit it raises `hit` on that entry and clears it on every other.
typedef struct AcropolisBridgeHotspot {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s16 id; // list terminator is -1
    /* 0xA */ u8  promptKind;
    /* 0xB */ s8  hit;
} AcropolisBridgeHotspot;

extern AcropolisBridgeHotspot D_acropolis_bridge_8018983C[];
extern s32                    D_acropolis_bridge_801917A8;

void func_acropolis_bridge_8017E60C(s32 arg0, s32 arg1);

#endif
