#ifndef PAD_H
#define PAD_H

#include "common.h"

// =============================================================================
// Types — controller state (src/main/pad.c, src/main/padutil.c)
// =============================================================================

/// 4-byte pad event entry in PadState banks at 0x10 / 0x30 (see Pad_ClearEvents,
/// func_8002C8E4). Cleared as sb/sb/sh of zero.
typedef struct _PadEvent {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} PadEvent;
STATIC_ASSERT_SIZEOF(PadEvent, 0x4);

/// Element of BSS array Pad_States (2 entries, total 0xB8).
/// Indexed with stride 0x5C (see Pad_SetCooldown). field_0 is initialised to
/// 0xFF by Pad_Init (pad status halfword); field_3 is set to 1 there.
/// field_4 / field_6 / field_8 are pad button masks (see Pad_CheckSpecialCombo /
/// Pad_CheckButtons); field_A is a counter/flag written by pad-related setup
/// (Pad_SetCooldown, func_8003FCF8, func_8003FC8C). field_2 is a ring index into
/// field_10 banks (func_8002C8E4). field_10 holds two banks of 8 pad-event
/// entries at 0x10 and 0x30. field_5A / field_5B are cleared during pad init.
typedef struct _PadState {
    /* 0x00 */ u16      field_0;
    /* 0x02 */ u8       field_2;
    /* 0x03 */ u8       field_3;
    /* 0x04 */ u16      field_4;
    /* 0x06 */ u16      field_6;
    /* 0x08 */ u16      field_8;
    /* 0x0A */ u8       field_A;
    /* 0x0B */ byte     unknown_B[0x5];
    /* 0x10 */ PadEvent field_10[2][8];
    /* 0x50 */ byte     unknown_50[0xA];
    /* 0x5A */ u8       field_5A;
    /* 0x5B */ u8       field_5B;
} PadState;
STATIC_ASSERT_SIZEOF(PadState, 0x5C);

/// Raw libpad port buffer (PadInitDirect targets). Indexed with stride 0x24.
/// field_2/field_3 are combined as a big-endian halfword by Pad_ReadButtonsInv.
typedef struct _PadRawPort {
    /* 0x00 */ byte unknown_0[0x2];
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   field_3;
    /* 0x04 */ byte unknown_4[0x20];
} PadRawPort;
STATIC_ASSERT_SIZEOF(PadRawPort, 0x24);

// =============================================================================
// Functions — src/main/pad.c
// =============================================================================

void Pad_Init(void);
/// Initialise GStruct27 list heads (D_800711B8 / D_800711C0); lives next to pad init.
void func_80028718(void);

// =============================================================================
// Functions — src/main/padutil.c
// =============================================================================

s32  Pad_CheckButtons(s32 port, s32 mode, s32 mask);
void Pad_SetCooldown(s32 port);
void Pad_ClearCooldown(s32 port);
s32  Pad_ReadButtonsInv(s32 port);
void Pad_ClearEvents(s32 port);
s32  Pad_CheckSpecialCombo(void);

// =============================================================================
// Globals
// =============================================================================

extern volatile PadState Pad_States[2];
extern PadRawPort        Pad_RawPorts[2];
/// Cleared by func_80028718 during system init.
extern s32 D_80071210;

#endif // PAD_H
