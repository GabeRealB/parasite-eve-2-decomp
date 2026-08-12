#ifndef PAD_H
#define PAD_H

#include "common.h"

// =============================================================================
// Types — controller state (src/main/pad.c, src/main/padutil.c)
// =============================================================================

/// 4-byte pad event entry in PadState banks at 0x10 / 0x30 (see Pad_ClearEvents,
/// Pad_PostEvent). Cleared as sb/sb/sh of zero.
typedef struct _PadEvent {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u16 field_2;
} PadEvent;
STATIC_ASSERT_SIZEOF(PadEvent, 0x4);

/// Element of BSS array Pad_States (2 entries, total 0xB8).
/// Indexed with stride 0x5C (see Pad_SetCooldown). status is initialised to
/// 0xFF by Pad_Init (pad status halfword); initialized is set to 1 there.
/// buttons / prevButtons / triggered are pad button masks (see Pad_CheckSpecialCombo /
/// Pad_CheckButtons); cooldown is a counter (Pad_SetCooldown /
/// Pad_UpdatePort0). autoRepeat is a timer for face/d-pad bits
/// (Pad_UpdatePort0). eventIdx is a ring index into events banks
/// (Pad_PostEvent). events holds two banks of 8 pad-event entries at 0x10
/// and 0x30. field_50..field_56 are analog stick related (cleared/read by
/// Pad_UpdatePort0 when status == 0x73). field_5A / field_5B are cleared
/// during pad init.
typedef struct _PadState {
    /* 0x00 */ s16      status;
    /* 0x02 */ u8       eventIdx;
    /* 0x03 */ u8       initialized;
    /* 0x04 */ u16      buttons;
    /* 0x06 */ u16      prevButtons;
    /* 0x08 */ u16      triggered;
    /* 0x0A */ u8       cooldown;
    /* 0x0B */ u8       autoRepeat;
    /* 0x0C */ byte     unknown_C[0x4];
    /* 0x10 */ PadEvent events[2][8];
    /* 0x50 */ s16      field_50;
    /* 0x52 */ s16      field_52;
    /* 0x54 */ s16      field_54;
    /* 0x56 */ s16      field_56;
    /* 0x58 */ byte     unknown_58[0x2];
    /* 0x5A */ u8       field_5A;
    /* 0x5B */ u8       field_5B;
} PadState;
STATIC_ASSERT_SIZEOF(PadState, 0x5C);

/// 6-byte scratch block allocated from G_SCRATCH_HEAD by Pad_UpdatePort0.
/// rawLo/rawHi hold PadRawPort.field_3/field_2 (little-endian halfword),
/// inverted into buttons; prevButtons is the previous frame's field_4.
typedef struct _PadScratch {
    /* 0x0 */ u16 buttons;
    /* 0x2 */ u16 prevButtons;
    /* 0x4 */ u8  rawLo;
    /* 0x5 */ u8  rawHi;
} PadScratch;
STATIC_ASSERT_SIZEOF(PadScratch, 0x6);

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
/// Initialise TmdObject list heads (Tmd_List / Tmd_ListAlt); lives next to pad init.
void Tmd_InitLists(void);

// =============================================================================
// Functions — src/main/padutil.c
// =============================================================================

s32  Pad_CheckButtons(s32 port, s32 mode, s32 mask);
void Pad_PostEvent(s32 port, s32 bank, s32 arg2, s32 arg3);
void Pad_SetCooldown(s32 port);
void Pad_ClearCooldown(s32 port);
s32  Pad_ReadButtonsInv(s32 port);
void Pad_ClearEvents(s32 port);
s32  Pad_CheckSpecialCombo(void);
void Pad_UpdatePort0(void);

// =============================================================================
// Types — pad remap (GameMain_Init zeros Pad_RemapState)
// =============================================================================

/// 0x1C-byte block; field_8 remaps pad input (Pad_UpdatePort0).
typedef struct _PadRemapState {
    /* 0x00 */ byte unknown_0[0x8];
    /* 0x08 */ s8   field_8;
    /* 0x09 */ byte unknown_9[0x13];
} PadRemapState;
STATIC_ASSERT_SIZEOF(PadRemapState, 0x1C);

// =============================================================================
// Globals
// =============================================================================

extern volatile PadState Pad_States[2];
extern PadRawPort        Pad_RawPorts[2];
extern PadRemapState*    Pad_RemapState;
/// Cleared by Tmd_InitLists during system init.
extern s32 D_80071210;

#endif // PAD_H
