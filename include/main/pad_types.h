#ifndef MAIN_PAD_TYPES_H
#define MAIN_PAD_TYPES_H

#include "common.h"

enum {
    PAD_VIBRATION_INACTIVE       = 0,
    PAD_VIBRATION_ACTIVE         = 1,
    PAD_VIBRATION_MOTOR_BINARY   = 0,
    PAD_VIBRATION_MOTOR_VARIABLE = 1,
};

/// Timed vibration contribution for one controller motor.
///
/// `PadState::events` holds eight requests per motor. The binary motor is on
/// when any active request has nonzero intensity; the variable motor uses the
/// greatest active intensity. The countdown advances on serviced controller
/// polls, normally once per VSync, and pauses while polling skips the controller.
/// A request still contributes on the poll that decrements its countdown to zero.
///
/// `Pad_PostEvent` doubles a signed halfword duration and stores the low halfword
/// without clamping; intensity is narrowed to a byte. Slots belong to the
/// resident controller state and can be replaced by later requests. Expiration
/// clears only `active`, so the other fields are meaningful only while active.
typedef struct {
    u8  active;         // Slot state (0 inactive, 1 active)
    u8  intensity;      // Motor drive (bank 0: 0 off, nonzero on; bank 1: 0..255)
    s16 pollsRemaining; // Serviced controller polls left; expires after decrement to zero
} PadVibrationRequest;
STATIC_ASSERT_SIZEOF(PadVibrationRequest, 0x4);

/// Element of BSS array Pad_States (2 entries, total 0xB8).
/// Indexed with stride 0x5C (see Pad_SetCooldown). status is initialised to
/// 0xFF by Pad_Init (pad status halfword); initialized is set to 1 there.
/// buttons / prevButtons / triggered are pad button masks (see Pad_CheckSpecialCombo /
/// Pad_CheckButtons); cooldown is a counter (Pad_SetCooldown /
/// Pad_UpdatePort0). autoRepeat is a timer for face/d-pad bits
/// (Pad_UpdatePort0). eventIdx is a ring index into events banks
/// (Pad_PostEvent). events holds eight vibration requests per motor (bank 0
/// binary, bank 1 variable intensity). field_50..field_56 are analog stick
/// related (cleared/read by Pad_UpdatePort0 when status == 0x73). field_5A / field_5B are cleared
/// during pad init.
typedef struct _PadState {
    /* 0x00 */ s16                 status;
    /* 0x02 */ u8                  eventIdx;
    /* 0x03 */ u8                  initialized;
    /* 0x04 */ u16                 buttons;
    /* 0x06 */ u16                 prevButtons;
    /* 0x08 */ u16                 triggered;
    /* 0x0A */ volatile u8         cooldown;
    /* 0x0B */ u8                  autoRepeat;
    /* 0x0C */ byte                unknown_C[0x4];
    /* 0x10 */ PadVibrationRequest events[2][8];
    /* 0x50 */ s16                 field_50;
    /* 0x52 */ s16                 field_52;
    /* 0x54 */ s16                 field_54;
    /* 0x56 */ s16                 field_56;
    /* 0x58 */ byte                unknown_58[0x2];
    /* 0x5A */ u8                  field_5A;
    /* 0x5B */ u8                  field_5B;
} PadState;
STATIC_ASSERT_SIZEOF(PadState, 0x5C);

/// 6-byte scratch block allocated from the scratch stack by Pad_UpdatePort0.
/// rawLo/rawHi hold PadRawPort.field_3/field_2 (little-endian halfword),
/// inverted into buttons; prevButtons is the previous frame's field_4.
typedef struct _PadScratch {
    /* 0x0 */ u16 buttons;
    /* 0x2 */ u16 prevButtons;
    /* 0x4 */ u8  rawLo;
    /* 0x5 */ u8  rawHi;
} PadScratch;
STATIC_ASSERT_SIZEOF(PadScratch, 0x6);

/// 0x1C-byte block; field_8 remaps pad input (Pad_UpdatePort0).
typedef struct _PadRemapState {
    /* 0x00 */ byte unknown_0[0x1];
    /* 0x01 */ u8   field_1; // stage/debug selector; 0x13 enables the light-probe path in Gp_DebugPanTask
    /* 0x02 */ byte unknown_2[0x1];
    /* 0x03 */ u8   field_3; // cleared by Gp_LoadFinishTask teardown
    /* 0x04 */ byte unknown_4[0x4];
    /* 0x08 */ s8   field_8; // remap mode: 0 off, -1 replay stream, 1 func_807150F8
    /* 0x09 */ s8   field_9; // 1: Gp_InitPlayClock calls func_80715198 when gDisplayState.demoScene is 0
    /* 0x0A */ s8   field_A; // nonzero: skip HUD ammo draw (Gp_DrawItemPrompt)
    /* 0x0B */ byte unknown_B[0x11];
} PadRemapState;
STATIC_ASSERT_SIZEOF(PadRemapState, 0x1C);

#endif // MAIN_PAD_TYPES_H
