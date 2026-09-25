#ifndef ROOMS_SHELTER_R47_H
#define ROOMS_SHELTER_R47_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

/// Work block of the room's first cap script: the task family whose state
/// table is `D_shelter_r47_8017D6C8` (dispatcher `func_shelter_r47_80182B18`).
/// `memCalloc(0x54)` in its state-0 entry `func_shelter_r47_8018138C`, stored
/// at `Task::work`.
typedef struct {
    s16 field_0[5]; ///< x of each of the five rows, eased toward a per-row target
    u8  pad_A[2];
    s16 field_C[5]; ///< y of each of the five rows
    u8  pad_16[2];
    s16 toggles[5]; ///< one-bit toggles, committed to game flags 0xAC, 0xD5, 0xAE, 0xD6 and 0xD2 when the script ends
    u8  pad_22[2];
    s16 field_24;
    s16 field_26;
    s16 field_28;
    s16 field_2A;
    s16 field_2C;
    s16 field_2E;
    s16 field_30;
    s16 field_32;
    /// Id of the hotspot the player confirmed. Its high byte is the kind of
    /// action, its low byte the entry the action applies to.
    union {
        s16 id;
        u16 raw;
        u8  index;
    } selection;
    u16 fade; ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    u8  pad_38[2];
    s16 field_3A;
    s16 field_3C;
    s16 field_3E;
    s16 field_40;
    s16 field_42;   ///< counter gating the move to state 3
    s16 step;       ///< sub-step selected by the running cap event
    s16 field_46;   ///< background scroll, clamped to 0..0x140
    s16 field_48;   ///< position in the `step` byte sequence being drawn
    s8  promptKind; ///< `promptKind` of the confirmed hotspot, forwarded to `func_800D4E78`
    u8  pad_4B[3];
    u8  field_4E;   ///< low byte of the area view saved on entry
    s8  field_4F;   ///< toggle a step flips: the low byte of the last selection accepted with kind 0
    s8  field_50;   ///< previous `field_4F`, kept when a new one is accepted
    s8  field_51;
    s8  field_52;   ///< mirrors toggle 3; bit 0 scrolls the background back toward 0
    u8  pad_53;
} ShelterR47State;
STATIC_ASSERT_SIZEOF(ShelterR47State, 0x54);

/// Work block of the room's second cap script: the task family whose state
/// table is `D_shelter_r47_8017D7DC` (dispatcher `func_shelter_r47_80185214`).
/// `memCalloc(0x30)` in its state-0 entry `func_shelter_r47_8018431C`, stored
/// at `Task::work`.
typedef struct {
    u8              pad_0[4];
    OverlayHotspot* hotspots; ///< table hit-tested against the action cursor
    u8              pad_8[2];
    s16             field_A;  ///< width of the first quad drawn by `func_shelter_r47_80183B84`; eases toward `field_E`
    s16             field_C;  ///< height of that quad; eases toward `field_10`
    s16             field_E;
    s16             field_10;
    s16             field_12; ///< width of the second quad drawn by `func_shelter_r47_80183B84`; eases toward `field_16`
    s16             field_14; ///< height of that quad; eases toward `field_18`
    s16             field_16;
    s16             field_18;
    s16             field_1A;   ///< id of the confirmed hotspot
    s16             field_1C;   ///< index of the map page shown, wrapping over 0..4
    s16             field_1E;   ///< target that `field_20` eases toward by a quarter of the gap a frame
    s16             field_20;   ///< x of the sprite drawn by `func_shelter_r47_80183FF4`
    u16             fade;       ///< fade-to-black ramp: +0x10 a frame, clamped at 0xFF
    s16             field_24;   ///< frame counter; past 300 the task moves to state 9
    u16             field_26;   ///< frames the first quad has been fully open; zeroed while it grows
    s8              promptKind; ///< `promptKind` of the confirmed hotspot, forwarded to `func_800D4E78`
    u8              field_29;   ///< low byte of the area view saved on entry
    s8              field_2A;
    s8              field_2B;   ///< non-zero holds the prompt off
    s8              field_2C;   ///< countdown; a sound plays as it reaches zero
    u8              pad_2D[3];
} ShelterR47State2;
STATIC_ASSERT_SIZEOF(ShelterR47State2, 0x30);

/// Latches set the first time the second cap script's hotspots 2 and 3 play
/// their one-off cap events.
extern u8 D_shelter_r47_8018A696;
extern u8 D_shelter_r47_8018A697;

s32  func_shelter_r47_8018097C(Task* task);
s32  func_shelter_r47_80180C48(Task* task);
void func_shelter_r47_80180F38(s16 x, s16 y, s16 id);
void func_shelter_r47_8018138C(Task* task);
void func_shelter_r47_80181568(Task* task);
void func_shelter_r47_80181914(Task* task, s16 arg1);
void func_shelter_r47_80182AA0(Task* task);
s32  func_shelter_r47_80182B9C(Task* task, OverlayHotspot* table, s16 x, s16 y);
void func_shelter_r47_80183210(void);
void func_shelter_r47_80183B84(Task* task);
void func_shelter_r47_80183E24(void);
void func_shelter_r47_80183F0C(void);
void func_shelter_r47_80183FF4(Task* task, s16 arg1);
void func_shelter_r47_80184124(Task* task, s16 arg1);

#endif // ROOMS_SHELTER_R47_H
